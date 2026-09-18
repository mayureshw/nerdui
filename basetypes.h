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
    kwd_dat         = "dat",
    kwd_eid         = "eid",
    kwd_buttons     = "buttons",
    kwd_action      = "action",
    kwd_choice      = "choice",
    kwd_field       = "field",
    kwd_field_name  = "field-name",
    kwd_field_value = "field-value",
    kwd_label       = "label",
    kwd_div         = "div",
    kwd_span        = "span",
    kwd_back        = "Back",
    kwd_next        = "Next",
    kwd_done        = "Done",
    kwd_no_data     = "No data.",
    kwd_empty       = "";

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

enum e_event_type
{
    E_SET,
    E_NEXT,
    E_BACK,
    E_DONE,
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

class EventHandler : public Type
{
public:
    virtual void handleEvent(e_event_type,string_view)=0;
};

class Response
{
    ostringstream _resp;
    bool _found_input = false;
    EventHandler *_eh = nullptr;
    EventHandler *_last_eh = nullptr;
    bool _have_button = false;
public:
    HtmlFormatter hf {_resp};
    EventHandler* eh() { return _eh; }
    EventHandler* last_eh() { return _last_eh; }
    // clear after sending every response
    void clear()
    {
        _resp.str("");
        _resp.clear();
        _found_input = false;
        _have_button = false;
    }
    // reset after resetting a session
    // clear would be called by the previous response, no need to repeat
    void reset()
    {
        setLast(nullptr);
        resetLast();
    }
    void resetLast()
    {
        _last_eh = nullptr;
    }
    void setLast(EventHandler *eh)
    {
        _last_eh = eh;
    }
    void foundInput(EventHandler *eh)
    {
        _eh = eh;
        _found_input = true;
    }
    void addedButton() { _have_button = true; }
    bool isInputFound() { return _found_input; }
    string str() { return _resp.str(); }
    string ec(e_event_type e) { return to_string(e); }
    void buttons()
    {
        hf.tag_open(kwd_div,{kwd_buttons,kwd_action});
        if ( _last_eh != nullptr )
            hf.button(kwd_eid,ec(E_BACK),kwd_back);
        if ( not _have_button )
        {
            if ( isInputFound() )
                hf.button(kwd_eid,ec(E_NEXT),kwd_next);
            else
                hf.button(kwd_eid,ec(E_DONE),kwd_done);
        }
        hf.tag_close(kwd_buttons);
    }
};

class ElementaryType : public EventHandler
{
protected:
    enum e_scalar_state
    {
        S_NOTSET,
        S_SETOK,
        S_SETERR,
    };
    e_scalar_state _state = S_NOTSET;
public:
    virtual void set(string_view)=0;
    void handleEvent(e_event_type event, string_view dat)
    {
        switch(event)
        {
        case E_SET:
        case E_NEXT:
            set(dat);
            break;
        case E_BACK:
            _state = S_NOTSET;
            break;
        case E_DONE:
            break;
        }
    }
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
        switch(_state) {
        case S_NOTSET:
            getInputWidget(resp,adescr);
            resp.foundInput(this);
            break;
        case S_SETOK:
        case S_SETERR:
            getPreview(resp,adescr);
            resp.setLast(this);
        }
    }
};

class String : public ElementaryType
{
    string _val;
public:
    void set(string_view val)
    {
        _val = val;
        _state = S_SETOK;
        clearErr();
    }
    void getInputWidget(Response& resp, string_view adescr)
    {
        resp.hf.textinput(adescr,kwd_dat);
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
        _state = S_SETOK;
        clearErr();
    }
public:
    E val() { return _val; }
    void getInputWidget(Response& resp, string_view adescr)
    {
        resp.hf.tag_open(kwd_label,{kwd_field});
        resp.hf.fieldname( adescr );

        if constexpr (D::_choice_widget == e_choice_widget::dropdown)
        {
            resp.hf.select_open(kwd_dat);
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
                resp.hf.radio(kwd_dat, D::_codes[i], D::_vdescr[i]);
        }
        else if constexpr (D::_choice_widget == e_choice_widget::button)
        {
            resp.hf.tag_open(kwd_div,{kwd_buttons,kwd_choice});
            for (size_t i = 0; i < D::_domainsz; i++)
                resp.hf.button(kwd_dat, D::_codes[i], D::_vdescr[i]);
            resp.hf.tag_close(kwd_div);
            resp.addedButton();
        }
        resp.hf.tag_close(kwd_label);
        resp.hf.nl();
    }
    string_view get_value_view() { return vdescr(); }
    void set(string_view val)
    {
        auto it = D::_codeval.find(val);
        if ( it != D::_codeval.end() )
        {
            _set(it->second);
            return;
        }
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

template <typename T, size_t card_min, size_t card_max>
class Array : public Type
{
    enum e_vector_state
    {
        S_NOTVISITED,
        S_PREVIEWING,
        S_ADDING,
        S_PASSEDOVER,
    };
    vector<T> _arr;
    e_vector_state _state = S_NOTVISITED;
    void arrayPreview(Response& resp)
    {
        if ( _arr.size() == 0 )
            resp.hf.span(kwd_no_data);
        else
            resp.hf.span("array preview todo");
    }
public:
    template<typename ContainedIn, size_t ordpos>
    void getResponse(Response& resp)
    {
        arrayPreview(resp);
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

    static constexpr bool is_scalar    = ( card_max == 1 );

    using AttrTyp =
        conditional_t< is_scalar, T, Array<T,card_min,card_max> >;

    AttrTyp _val;
public:
    template<typename ContainedInParent, size_t ordposParent>
    void getResponse(Response& resp)
    {
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
