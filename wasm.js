import { WASI, File, OpenFile, ConsoleStdout } from "https://esm.sh/@bjorn3/browser_wasi_shim@0.3.0";

let wasm = null;
let wasi = null;

// -------------------------------------------------------------------
// Load + instantiate the module once.
// -------------------------------------------------------------------
async function loadWasm() {
  // Minimal WASI environment: no real filesystem, stdout/stderr -> console.
  // fd 0/1/2 must exist even if unused, or early libc init can trap.
  wasi = new WASI([], [], [
    new OpenFile(new File([])),                 // stdin
    ConsoleStdout.lineBuffered(msg => console.log("[wasm stdout]", msg)),
    ConsoleStdout.lineBuffered(msg => console.error("[wasm stderr]", msg)),
  ]);

  const imports = {
    wasi_snapshot_preview1: wasi.wasiImport,
    env: {},
  };

  const res = await fetch("app.wasm");
  const ctype = res.headers.get("content-type") || "";

  if (!res.ok) {
    throw new Error(
      `fetch("app.wasm") returned HTTP ${res.status} -- check the file ` +
      `is actually served at this path (not a dev-server SPA fallback).`
    );
  }
  if (!ctype.includes("wasm")) {
    console.warn(
      `Warning: server sent Content-Type "${ctype}" instead of ` +
      `"application/wasm". Falling back to non-streaming instantiate; ` +
      `you should still fix the server/static-file config.`
    );
  }

  // Non-streaming instantiate works even when Content-Type is wrong,
  // as long as the bytes themselves are a real wasm module.
  const bytes = await res.arrayBuffer();
  // Cheap sanity check: real wasm binaries start with \0asm.
  const magic = new Uint8Array(bytes.slice(0, 4));
  if (!(magic[0] === 0x00 && magic[1] === 0x61 && magic[2] === 0x73 && magic[3] === 0x6d)) {
    throw new Error(
      "app.wasm did not start with the wasm magic bytes (\\0asm) -- " +
      "you almost certainly received an HTML error page instead of the " +
      "compiled module. Check the path and that you're not opening this " +
      "file over file:// (serve it over http:// instead)."
    );
  }

  const { instance } = await WebAssembly.instantiate(bytes, imports);

  wasm = instance.exports;
  if (!wasm.memory) {
    throw new Error(
      "instance.exports.memory is missing — link with --export-memory"
    );
  }

  // Reactor modules (a library like this one, built with
  // -mexec-model=reactor) export _initialize instead of _start.
  // This must run before calling handle_request/wasm_alloc/etc.,
  // since it's what sets up the libc heap and wires WASI to memory.
  if (typeof wasm._initialize === "function") {
    wasi.initialize(instance);
  } else if (typeof wasm._start === "function") {
    // Command-style module (has main()) — start() runs _start for you.
    // Only do this once; calling it twice re-runs libc init.
    wasi.start(instance);
  } else {
    console.warn(
      "No _initialize or _start export found -- if calls into libc " +
      "(malloc, etc.) crash, your build likely needs " +
      "-Wl,--export=_initialize (pass -mexec-model=reactor at compile time)."
    );
  }

  return wasm;
}

// Always re-wrap the buffer: wasm_alloc/handle_request can call malloc,
// which can grow memory and detach any previously-taken view.
function memView() {
  return new Uint8Array(wasm.memory.buffer);
}

// Write a JS string into wasm-owned memory (allocated via wasm_alloc).
// Returns { ptr, len } — caller is responsible for wasm_free(ptr).
function writeString(str) {
  const bytes = new TextEncoder().encode(str);
  const ptr = wasm.wasm_alloc(bytes.length);
  if (!ptr) throw new Error("wasm_alloc returned null (OOM?)");
  memView().set(bytes, ptr);
  return { ptr, len: bytes.length };
}

// Read a null-terminated string out of wasm memory starting at ptr.
function readCString(ptr) {
  const mem = memView();
  let end = ptr;
  while (mem[end] !== 0) end++;
  return new TextDecoder().decode(mem.subarray(ptr, end));
}

// -------------------------------------------------------------------
// The actual call.
// -------------------------------------------------------------------
async function query(qs) {
  if (!wasm) await loadWasm();

  const { ptr: inPtr, len } = writeString(qs);
  let outPtr = 0;
  try {
    outPtr = wasm.handle_request(inPtr, len);
    if (!outPtr) throw new Error("handle_request returned null");
    return readCString(outPtr);
  } finally {
    // Free both buffers regardless of success/failure.
    wasm.wasm_free(inPtr);
    if (outPtr) wasm.wasm_free(outPtr);
  }
}

// -------------------------------------------------------------------
// Wire up the page.
// -------------------------------------------------------------------
const statusEl = document.getElementById("status");
const outEl = document.getElementById("out");

async function runQuery(qs) {
  console.log("[runQuery] sending qs:", qs);
  statusEl.textContent = "";
  try {
    const html = await query(qs);
    console.log("[runQuery] got response, length:", html.length);
    // If sm.getResponse() returns an HTML fragment meant for embedding,
    // render it directly instead of escaping it as text:
    outEl.innerHTML = html;
  } catch (e) {
    console.error("[runQuery] failed:", e);
    statusEl.textContent = "error: " + e.message;
  }
}

// The injected app HTML brings its own <form>s and <a href="?...">
// navigation links (by design, per the zero-JS server round-trip model).
// Intercept both here so a "next step" click stays inside this harness
// and goes through handle_request() again, instead of the browser doing
// a real HTTP navigation back to lighttpd.
outEl.addEventListener("submit", (e) => {
  const form = e.target.closest("form");
  if (!form) return;
  console.log("[submit] intercepted, submitter:", e.submitter, "action:", form.getAttribute("action"));
  e.preventDefault();

  const method = (form.getAttribute("method") || "get").toLowerCase();
  // Pass e.submitter as FormData's second arg -- otherwise a clicked
  // <button type="submit" name="TxnTyp" value="CR"> never makes it into
  // the collected fields, since FormData(form) alone only reads named
  // inputs/selects/textareas, not the button that triggered submission.
  const params = new URLSearchParams(new FormData(form, e.submitter));

  // Support <form action="?step=2"> style actions where some params
  // are baked into the action URL itself, plus whatever the fields add.
  const actionUrl = new URL(form.getAttribute("action") || "", location.href);
  for (const [k, v] of params) actionUrl.searchParams.append(k, v);

  // handle_request only sees a flat query string either way, regardless
  // of whether the form declared method="get" or "post".
  runQuery(actionUrl.search.slice(1));
});

outEl.addEventListener("click", (e) => {
  const a = e.target.closest("a[href]");
  if (!a) return;

  const url = new URL(a.getAttribute("href"), location.href);
  // Only intercept same-origin app navigation; let external links
  // (docs, mailto:, etc.) behave normally.
  if (url.origin !== location.origin) return;

  e.preventDefault();
  runQuery(url.search.slice(1));
});

// Load the module, then immediately make the first request with an
// empty query string -- this mirrors a plain GET / with no params,
// so the app renders its initial/landing state without waiting for
// a user click.
loadWasm()
  .then(() => runQuery(""))
  .catch(e => {
    statusEl.textContent = "failed to load wasm: " + e.message;
  });
