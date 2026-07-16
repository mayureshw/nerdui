#ifndef _WASM_BRIDGE_H_
#define _WASM_BRIDGE_H_

// WASM interface

#include <cstdlib>
#include <cstring>
#include <salzaverde/query.h>
#include <string>

extern "C" {
    char* wasm_alloc(size_t n) { return (char*)malloc(n); }
    void  wasm_free(char* p)    { free(p); }
    char* handle_request(const char* qs, size_t len)
    {
        string input(qs, len);
        auto q = Query::parse(input);
        auto resp = sm.getResponse(q);
        char* out = (char*)malloc(resp.size() + 1);
        memcpy(out, resp.c_str(), resp.size() + 1);
        return out;
    }
}

#endif
