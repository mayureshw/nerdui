#ifndef _BASETYPES_H
#define _BASETYPES_H

#include <string>
#include <string_view>
#include <sstream>
#include <iostream>
#include <array>
#include <variant>
#include <frozen/unordered_map.h>
#include <frozen/string.h>
using namespace std;

// Common pattern to use with visit
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

class HtmlFormatter
{
    ostream& _os;
public:
    void text(string_view s) { _os << s; }
    template <typename... Args> void text(Args&&... args)
    { (_os << ... << forward<Args>(args)); }
    void nl() { _os << "\n"; }
    void li_open() { _os << "<li>"; }
    void li_close() { _os << "</li>"; }
    void ul_open() { _os << "<ul>"; }
    void ul_close() { _os << "</ul>"; }
    void p_open() { _os << "<p>"; }
    void p_close() { _os << "</p>"; }
    template <typename... Args> void p(Args&&... args)
    { p_open(); text(forward<Args>(args)...); p_close(); }
    void select_open(string_view name) { _os << "<select name=\"" << name << "\">"; }
    void select_close() { _os << "</select>"; }
    void option(string_view code, string_view s)
    { _os << "<option value=\"" << code << "\">" << s << "</option>"; }
    HtmlFormatter(ostream& os) : _os(os) {}
};

class Settable
{
public:
    virtual void set(string_view)=0;
    virtual string fieldname()=0;
};

class Response
{
    ostringstream _resp;
    bool _found_input = false;
    Settable *_settable = nullptr;
public:
    HtmlFormatter hf {_resp};
    Settable* settable() { return _settable; }
    void clear()
    {
        _resp.str("");
        _resp.clear();
        _found_input = false;
    }
    void foundInput(Settable *settable)
    {
        _settable = settable;
        _found_input = true;
    }
    bool isInputFound() { return _found_input; }
    string str() { return _resp.str(); }
};

template <typename D, typename E> class Domain : public Settable
{
    E _val;
    bool _is_set = false;
    int index() { return static_cast<int>(_val); }
    void getInputWidget(Response& resp)
    {
        resp.hf.text(D::_descr,": ");
        resp.hf.select_open(D::_name);
        resp.hf.nl();

        for (size_t i = 0; i < D::_domainsz; i++)
        {
            resp.hf.option(D::_codes[i], D::_vdescr[i]);
            resp.hf.nl();
        }
        resp.hf.select_close();
        resp.hf.nl();
    }
public:
    E val() { return _val; }
    void set(string_view val)
    {
        _val = code2val(val);
        _is_set = true;
    }
    string fieldname() { return D::_name; }
    static bool isStruct() { return false; }
    string_view code() { return D::_codes[index()]; }
    string_view vdescr() { return D::_vdescr[index()]; }
    string_view descr() { return D::_descr; }
    E code2val(string_view& code)
    {
        auto it = D::_codeval.find(code);
        if ( it == D::_codeval.end() )
        {
            stringstream err;
            err << "Attempt to set invalid value for domain " << D::_name << "::" << code;
            throw domain_error( err.str() );
        }
        return it->second;
    }
    void getResponse(Response& resp)
    {
        if ( not _is_set )
        {
            getInputWidget(resp);
            resp.foundInput(this);
        }
        else resp.hf.text( descr(), ": ", vdescr() );
    }
};

template <typename T, typename SelectorType, typename... UnionOf> class Union
{
    T& tinst() { return static_cast<T&>(*this); }
protected:
    SelectorType& _selector;
public:
    variant<monostate,UnionOf...> _u;
    static bool isStruct() { return true; }
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

template <typename T> class Struct
{
    T& tinst() { return static_cast<T&>(*this); }
public:
    static bool isStruct() { return true; }
    void getResponse(Response& resp)
    {
        resp.hf.p(T::_descr,":");

        resp.hf.ul_open();
        resp.hf.nl();

        for( auto attr : tinst()._attribs )
        {
            resp.hf.li_open();
            resp.hf.nl();
            attr->getResponse(resp);
            resp.hf.li_close();
            resp.hf.nl();
            if( resp.isInputFound() ) break;
        }
        resp.hf.ul_close();
        resp.hf.nl();
    }
};

class BaseAttrib
{
public:
    virtual void getResponse(Response& resp)=0;
    virtual bool isStruct()=0;
    virtual ~BaseAttrib() = default;
};

template <typename T, int card_min, int card_max> class Attrib : public BaseAttrib
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
    void getResponse(Response& resp) requires (is_scalar) { _val.getResponse(resp); }
    bool isStruct() { return AttrTyp::isStruct(); }
    T& get() requires (card_max == 1) { return _val; }
    const T& get() const requires (card_max == 1) { return _val; }

    Attrib() requires (card_max != 1) = default;

    template<typename... Args> requires (card_max == 1)
    explicit Attrib(Args&&... args) : _val(forward<Args>(args)...)
    {}


};

#endif
