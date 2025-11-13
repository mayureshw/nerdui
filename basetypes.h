#ifndef _BASETYPES_H
#define _BASETYPES_H

#include <string>
#include <sstream>
#include <iostream>
#include <array>
#include <frozen/unordered_map.h>
#include <frozen/string.h>
using namespace std;

template <typename D, typename E> class Domain
{
    const E _val;
    int index() { return static_cast<int>(_val); }
public:
    string_view code() { return D::_codes[index()]; }
    string_view descr() { return D::_descr[index()]; }
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

template <typename T> class Struct
{
};

template <typename T> class Attrib
{
};

#endif
