#ifndef _BASETYPES_H
#define _BASETYPES_H

#include <string>
#include <string_view>
#include <sstream>
#include <iostream>
#include <array>
#include <variant>
#include <tuple>
#include <frozen/unordered_map.h>
#include <frozen/string.h>
using namespace std;

// Common pattern to use with visit
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

enum class e_choice_widget
{
    dropdown,
    radio,
    button,
};

enum class e_persistence_type
{
    transient,
    column,
    blob,
};

class ErrIf
{
    bool _has_err = false;
    string _err;
public:
    bool hasErr() { return _has_err; }
    void markErr(string err)
    {
        _err = err;
        _has_err = true;
    }
    void clearErr()
    {
        _err = "";
        _has_err = false;
    }
};

class Type : public ErrIf {};


class HtmlFormatter
{
    ostream& _os;
public:
    void text(string_view s) { _os << s; }
    template <typename... Args> void text(Args&&... args)
    { (_os << ... << std::forward<Args>(args)); }
    void nl() { _os << "\n"; }
    void br() { _os << "<br>\n"; }
    void li_open() { _os << "<li>"; }
    void li_close() { _os << "</li>"; }
    void ul_open() { _os << "<ul>"; }
    void ul_close() { _os << "</ul>"; }
    void p_open() { _os << "<p>"; }
    void p_close() { _os << "</p>"; }
    template <typename... Args> void p(Args&&... args)
    { p_open(); text(std::forward<Args>(args)...); p_close(); }
    void select_open(string_view name) { _os << "<select name=\"" << name << "\">"; }
    void select_close() { _os << "</select>"; }
    void option(string_view code, string_view s)
    { _os << "<option value=\"" << code << "\">" << s << "</option>"; }
    void radio(string_view name, string_view code, string_view s)
    {
        _os << "<label>"
            << "<input type=\"radio\" name=\""
            << name
            << "\" value=\""
            << code
            << "\"> "
            << s
            << "</label>";
        br();
    }
    void button(string_view name, string_view code, string_view descr)
    {
        _os << "<button type=\"submit\" name=\""
            << name
            << "\" value=\""
            << code
            << "\">"
            << descr
            << "</button>";
    }
    HtmlFormatter(ostream& os) : _os(os) {}
};

class Settable : public Type
{
protected:
    bool _is_set = false;
public:
    virtual void set(string_view)=0;
    static inline constexpr string_view kwd_fldid = "0";
};

class Response
{
    ostringstream _resp;
    bool _found_input = false;
    Settable *_settable = nullptr;
    bool _have_button = false;
public:
    HtmlFormatter hf {_resp};
    Settable* settable() { return _settable; }
    void clear()
    {
        _resp.str("");
        _resp.clear();
        _found_input = false;
        _have_button = false;
    }
    void foundInput(Settable *settable)
    {
        _settable = settable;
        _found_input = true;
    }
    void addedButton() { _have_button = true; }
    bool haveButton() { return _have_button; }
    bool isInputFound() { return _found_input; }
    string str() { return _resp.str(); }
};

class ElementaryType : public Settable
{
public:
    virtual void getInputWidget(Response&)=0;
    virtual void getPreview(Response&)=0;
    void getResponse(Response& resp)
    {
        if ( not _is_set )
        {
            getInputWidget(resp);
            resp.foundInput(this);
        }
        else getPreview(resp);
    }
};

class String : public ElementaryType
{
    string _val;
public:
    void set(string_view val) { _val = val; }
    void getInputWidget(Response& resp) {}
    void getPreview(Response& resp) {}
};

template <typename D, typename E> class Domain : public ElementaryType
{
    E _val;
    int index() { return static_cast<int>(_val); }
    void _set(E eval)
    {
        _val = eval;
        _is_set = true;
        clearErr();
    }
public:
    E val() { return _val; }
    void getInputWidget(Response& resp)
    {
        resp.hf.text(D::_descr);

        if constexpr (D::_choice_widget == e_choice_widget::dropdown)
        {
            resp.hf.select_open(kwd_fldid);
            resp.hf.nl();
            for (size_t i = 0; i < D::_domainsz; i++)
            {
                resp.hf.option(D::_codes[i], D::_vdescr[i]);
                resp.hf.nl();
            }
            resp.hf.select_close();
        }
        else if constexpr (D::_choice_widget == e_choice_widget::radio)
        {
            resp.hf.br();
            for (size_t i = 0; i < D::_domainsz; i++)
                resp.hf.radio(kwd_fldid, D::_codes[i], D::_vdescr[i]);
        }
        else if constexpr (D::_choice_widget == e_choice_widget::button)
        {
            for (size_t i = 0; i < D::_domainsz; i++)
                resp.hf.button(kwd_fldid, D::_codes[i], D::_vdescr[i]);
            resp.addedButton();
        }
        resp.hf.nl();
    }
    void getPreview(Response& resp)
    {
        resp.hf.text( descr(), vdescr() );
    }
    void set(string_view val)
    {
        // frozen's hash is seen not working on wasm
        // consider replacing frozen with gperf generated code
        // consider filing a bug report
        // sequential search is a stop gap (ok for small domains)
#ifdef WASM
        for( const auto& [k,v]: D::_codeval )
        {
            if ( k == val )
            {
                _set(v);
                return;
            }
        }
#else
        auto it = D::_codeval.find(val);
        if ( it == D::_codeval.end() )
        {
            _set(it->second);
            return;
        }
#endif
        markErr("Invalid value");
    }
    string_view code() { return D::_codes[index()]; }
    string_view vdescr() { return D::_vdescr[index()]; }
    string_view descr() { return D::_descr; }
};

template <typename T, typename SelectorType, typename... UnionOf> class Union : public Type
{
    T& tinst() { return static_cast<T&>(*this); }
protected:
    SelectorType& _selector;
public:
    variant<monostate,UnionOf...> _u;
    void getResponse(Response& resp)
    {
        return tinst().dispatch(
            [this,&resp]<typename VT>() {
            this->template getResponseImpl<VT>(resp);
            });
    }
    template<typename VT> void getResponseImpl(Response& resp)
    {
        if constexpr ( not is_same_v<VT,monostate> )
        {
            if (!holds_alternative<VT>(_u)) _u.template emplace<VT>();
            auto& v = get<VT>(_u);
            v.getResponse(resp);
        }
    }
    Union(SelectorType& selector) : _selector(selector) {}
};

template <typename T> class Struct : public Type
{
    T& tinst() { return static_cast<T&>(*this); }
public:
    void getResponse(Response& resp)
    {

        resp.hf.ul_open();
        resp.hf.nl();

        auto attribs_tup = tinst().attributes();

        apply([&](auto&... attr)
        {
            // fold expr on &&, so that it breaks when response is found
            ([&](auto&& a) {
                resp.hf.p_open();
                resp.hf.nl();
                a.getResponse(resp);
                resp.hf.p_close();
                resp.hf.nl();
                return not resp.isInputFound();
            }(attr) && ...);
        }, attribs_tup );

        resp.hf.ul_close();
        resp.hf.nl();
    }
};

template <
    typename ContainedIn,
    size_t ordpos,
    typename T,
    size_t card_min,
    size_t card_max,
    e_persistence_type persistence_type>
class Attrib //: public BaseAttrib
{
    static_assert(
        card_max == 1 || is_default_constructible_v<T>,
        "Non-scalar Attrib requires default-constructible T"
    );

    static constexpr bool is_scalar    = ( card_max == 1 );
    static constexpr bool is_unbounded = ( card_max < 0  );
    static constexpr bool is_bounded   = ( card_max > 1  );

    using AttrTyp =
        conditional_t< is_scalar, T,
        conditional_t< is_unbounded, vector<T>,
        array<T, card_max>>>;

    AttrTyp _val;
public:
    void getResponse(Response& resp)
    {
        static_assert( is_scalar, "getResponse not implemented for vectors" );
        _val.getResponse(resp);
    }
    T& get() requires (card_max == 1) { return _val; }
    const T& get() const requires (card_max == 1) { return _val; }

    Attrib() requires (card_max != 1) = default;

    template<typename... Args> requires (card_max == 1)
    explicit Attrib(Args&&... args) : _val(std::forward<Args>(args)...)
    {}


};

#endif
