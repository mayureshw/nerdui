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

template <typename D, typename E> class Domain
{
    const E _val;
    int index() { return static_cast<int>(_val); }
public:
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
    Domain(E val) : _val(val) {}
    Domain(string_view code) : _val(code2val(code)) {}
};

template <typename T, typename ST> class Union
{
    ST& _selector;
public:
    Union(ST& selector) : _selector(selector) {}
};

template <typename T> class Struct
{
};

class BaseAttrib
{
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
    Attrib() requires (card_max != 1) = default;

    template<typename... Args> requires (card_max == 1)
    explicit Attrib(Args&&... args) : _val(forward<Args>(args)...)
    {}


};

#endif
