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

class Response
{
    ostringstream _resp;
    bool _found_input = false;
public:
    void clear()
    {
        _resp.str("");
        _resp.clear();
        _found_input = false;
    }
    void foundInput() { _found_input = true; }
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

template <typename D, typename E> class Domain
{
    E _val;
    int index() { return static_cast<int>(_val); }
public:
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
    }
};

template<typename SelectorDomainEnum, SelectorDomainEnum Code, typename VariantType>
struct SelectorCase
{
    using t_Code = SelectorDomainEnum;
    static constexpr t_Code code = Code;
    using t_Variant = VariantType;
};

template <typename T, typename SelectorType, typename... Cases> class Union
{
using t_storage = std::variant<std::monostate, typename Cases::t_Variant...>;

    SelectorType& _selector;
    t_storage _u;
public:
    static bool isStruct() { return true; }
    void getResponse(Response& resp)
    {
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
