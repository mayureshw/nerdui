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

constexpr string_view
    kwd_fldid       = "0",
    kwd_buttons     = "buttons",
    kwd_field       = "field",
    kwd_field_name  = "field-name",
    kwd_field_value = "fiela-valued",
    kwd_label       = "label",
    kwd_div         = "div",
    kwd_span        = "span";

#include "htmlformatter.h"

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


// Placeholder type
class NoneType {};

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

class Settable : public Type
{
protected:
    bool _is_set = false;
public:
    virtual void set(string_view)=0;
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
    virtual void getInputWidget(Response&,string_view)=0;
    virtual string_view get_value_view()=0;
    void getPreview(Response& resp, string_view adescr)
    {
        resp.hf.fieldname( adescr );
        resp.hf.fieldvalue( get_value_view() );
    }
    template<typename ContainedIn, size_t ordpos>
    void getResponse(Response& resp)
    {
        constexpr string_view adescr = ContainedIn::_adescr[ordpos];
        if ( not _is_set )
        {
            getInputWidget(resp,adescr);
            resp.foundInput(this);
        }
        else getPreview(resp,adescr);
    }
};

class String : public ElementaryType
{
    string _val;
public:
    void set(string_view val)
    {
        _val = val;
        _is_set = true;
        clearErr();
    }
    void getInputWidget(Response& resp, string_view adescr)
    {
        resp.hf.textinput(adescr,kwd_fldid);
    }
    string_view get_value_view() { return _val; }
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
    void getInputWidget(Response& resp, string_view adescr)
    {
        resp.hf.tag_open(kwd_label,{kwd_field});
        resp.hf.span(adescr);

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
            resp.hf.tag_open(kwd_div,{kwd_buttons});
            for (size_t i = 0; i < D::_domainsz; i++)
                resp.hf.button(kwd_fldid, D::_codes[i], D::_vdescr[i]);
            resp.hf.tag_close(kwd_div);
            resp.addedButton();
        }
        resp.hf.tag_close(kwd_label);
        resp.hf.nl();
    }
    string_view get_value_view() { return vdescr(); }
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

template <typename T, typename SelectorType, typename... UnionOf>
class Union : public Type
{
    T& tinst() { return static_cast<T&>(*this); }
protected:
    SelectorType& _selector;
public:
    variant<monostate,UnionOf...> _u;
    template<typename ContainedIn, size_t ordpos>
    void getResponse(Response& resp)
    {
        return tinst().dispatch(
            [this,&resp]<typename VT>() {
            this->template getResponseImpl<ContainedIn,ordpos,VT>(resp);
            });
    }
    template<typename ContainedIn, size_t ordpos, typename VT>
    void getResponseImpl(Response& resp)
    {
        if constexpr ( not is_same_v<VT,monostate> )
        {
            if (!holds_alternative<VT>(_u)) _u.template emplace<VT>();
            auto& v = get<VT>(_u);
            v.template getResponse<ContainedIn,ordpos>(resp);
        }
    }
    Union(SelectorType& selector) : _selector(selector) {}
};

template <typename T> class Struct : public Type
{
    T& tinst() { return static_cast<T&>(*this); }
public:
    template<typename ContainedIn, size_t ordpos>
    void getResponse(Response& resp)
    {
        auto attribs_tup = tinst().attributes();
        apply([&](auto&... attr)
        {
            // fold expr on &&, so that it breaks when response is found
            ([&](auto&& a) {
                resp.hf.p_open();
                resp.hf.nl();
                a.template getResponse<NoneType,0>(resp);
                resp.hf.p_close();
                resp.hf.nl();
                return not resp.isInputFound();
            }(attr) && ...);
        }, attribs_tup );
    }
};

template <
    typename ContainedIn,
    size_t ordpos,
    typename T,
    size_t card_min,
    size_t card_max,
    e_persistence_type persistence_type>
class Attrib
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
    template<typename ContainedInParent, size_t ordposParent>
    void getResponse(Response& resp)
    {
        static_assert( is_scalar, "getResponse not implemented for vectors" );
        _val.template getResponse<ContainedIn,ordpos>(resp);
    }
    T& get() requires (card_max == 1) { return _val; }
    const T& get() const requires (card_max == 1) { return _val; }

    Attrib() requires (card_max != 1) = default;

    template<typename... Args> requires (card_max == 1)
    explicit Attrib(Args&&... args) : _val(std::forward<Args>(args)...)
    {}


};

#endif
