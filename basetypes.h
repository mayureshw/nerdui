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

namespace html {
    constexpr string_view
        select_open  = "<select name=\"",
        select_close = "</select>",
        opt_open     = "<option value=\"",
        opt_mid      = "\">",
        opt_close    = "</option>";
}

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
    template <typename T>
    Response& operator<<(const T& v)
    {
        _resp << v;
        return *this;
    }
    // Required to support manipulators like endl
    Response& operator<<(ostream& (*manip)(ostream&))
    {
        manip(_resp);
        return *this;
    }
    string str() { return _resp.str(); }
};

template <typename D, typename E> class Domain : public Settable
{
    E _val;
    bool _is_set = false;
    int index() { return static_cast<int>(_val); }
    void getInputWidget(Response& resp)
    {
        resp << html::select_open
             << D::_name
             << "\">"
             << endl;

        for (size_t i = 0; i < D::_domainsz; i++)
            resp << html::opt_open
                 << D::_codes[i]
                 << html::opt_mid
                 << D::_vdescr[i]
                 << html::opt_close
                 << endl;

        resp << html::select_close << endl;
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
        else resp << descr() << ": " << vdescr();
    }
};

template <typename T, typename SelectorType, typename... UnionOf> class Union
{
    T& tinst() { return static_cast<T&>(*this); }
protected:
    SelectorType& _selector;
    variant<monostate,UnionOf...> _u;
public:
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
        resp << "<p>" << T::_descr << ":</p>" << endl;
        resp << "<ul>" << endl;
        for( auto attr : tinst()._attribs )
        {
            resp << "<li>" << endl;
            attr->getResponse(resp);
            resp << "</li>" << endl;
            if( resp.isInputFound() ) break;
        }
        resp << "</ul>" << endl;
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
