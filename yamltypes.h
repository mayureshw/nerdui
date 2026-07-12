#ifndef _YAML_TYPES_H_
#define _YAML_TYPES_H_

#include "yamlif.h"

class Type : public YamlIf
{
protected:
    string _descr;
    void parse_descr(const Y_Node& node)
    {
        _descr = parse_dynamic_string(node);
    }
    void parse_kind(const Y_Node& node) {}
public:
    virtual void render(string_view,ostream&)=0;
    virtual bool is_union() { return false; }
    virtual ~Type() {}
};

class Domain : public Type
{
    static constexpr string_view _domain_tmpl = R"TMPL(
enum class e_{{name}} {
{{#values}}
    {{key}},
{{/values}}
};

class {{name}} : public Domain<{{name}},e_{{name}}>
{
public:
    constexpr static inline e_ChoiceWidget _choiceWidget = e_ChoiceWidget::{{choice_widget}};
    using t_dom = e_{{name}};
    constexpr static char _name[] = "{{name}}";
    constexpr static char _descr[] = "{{descr}}";
    constexpr static string_view _codes[] {
        {{#values}}
        "{{key}}",
        {{/values}}
        };
    constexpr static size_t _domainsz = sizeof(_codes)/sizeof(_codes[0]);
    constexpr static array<string_view,_domainsz> _vdescr {
        {{#values}}
        "{{value}}",
        {{/values}}
        };
    constexpr static frozen::unordered_map<frozen::string,t_dom,_domainsz> _codeval = {
        {{#values}}
        { "{{key}}", t_dom::{{key}} },
        {{/values}}
        };
};
)TMPL";

    map<string,string> _keyvals;
    string _choice_widget = "DropDown";
    void parse_values(const Y_Node& node)
    {
        handle_dynamic_map<string>(node, _keyvals, PARSE(dynamic_string));
    }
    void parse_choice_widget(const Y_Node& node)
    {
        _choice_widget = parse_static_string(node,dom_choice_widget);
    }
    mdata get_mdata(string_view name)
    {
        mdata d;
        d.set(string(kwd_name),string(name));
        d.set(string(kwd_descr),_descr);
        d.set(string(kwd_choice_widget),_choice_widget);

        mdata values;
        d.set(string(kwd_values),map_to_list(_keyvals));
        return d;
    }
public:
    void render(string_view name, ostream& os)
    {
        render_tmpl(_domain_tmpl,get_mdata(name),os);
    }
    Domain(const Y_Node& node)
    {
        HandlerMap<void> hmap
            {
                { kwd_descr, PARSE(descr) },
                { kwd_values, PARSE(values) },
                { kwd_choice_widget, PARSE(choice_widget) },
                { kwd_kind, PARSE(kind) },
            };
        KeySet mandatory { kwd_descr, kwd_values };
        handle_static_map(node, hmap, mandatory);
    }
};

class UnionCases : public YamlIf
{
    map<string,string> _keytyp;
    string parse_type(const Y_Node& node)
    {
        auto type = parse_dynamic_string(node);
        check_type_exists(node,type);
        return type;
    }
public:
    UnionCases(const Y_Node& node)
    {
        handle_dynamic_map<string>(node, _keytyp, PARSE(type));
    }
};

class Union : public Type
{
    string _selectorTyp;
    bool _selector_is_union;
    map<string,UnionCases*> _typ_cases;
    void parse_selector_typ(const Y_Node& node)
    {
        _selectorTyp = parse_dynamic_string(node);
        auto typ = check_type_exists(node,_selectorTyp);
        _selector_is_union = typ->is_union() ;
    }
    void parse_cases(const Y_Node& node)
    {
        handle_dynamic_map<UnionCases*>(node, _typ_cases,
            CREATE(UnionCases), check_type_exists);
    }
public:
    void render(string_view,ostream& os)
    {
    }
    bool is_union() { return true; }
    Union(const Y_Node& node)
    {
        HandlerMap<void> hmap
            {
                { kwd_descr, PARSE(descr) },
                { kwd_kind, PARSE(kind) },
                { kwd_selector_typ, PARSE(selector_typ) },
                { kwd_cases, PARSE(cases) },
            };
        KeySet mandatory { kwd_descr, kwd_selector_typ, kwd_cases };
        handle_static_map(node, hmap, mandatory);
    }
};

class Attrib;
using AttribMap = map<string,Attrib*>;

class Attrib : public YamlIf
{
    string _descr;
    string _type;
    string _selector;
    uint32_t _card_min = 0;
    uint32_t _card_max = 1;
    void parse_descr(const Y_Node& node)
    {
        _descr = parse_dynamic_string(node);
    }
    void parse_type(const Y_Node& node)
    {
        _type = parse_dynamic_string(node);
        check_type_exists(node,_type);
    }
    void parse_selector(const Y_Node& node, const AttribMap& attribs)
    {
        _selector = parse_dynamic_string(node);
        if ( attribs.find(_selector) == attribs.end() )
        {
            cerr << "Selector used before declaring " << _selector;
            print_err_loc(node);
            exit(1);
        }
    }
public:
    mdata get_mdata(string_view name)
    {
        mdata d;
        d.set(string(kwd_name),string(name));
        d.set(string(kwd_descr),_descr);
        d.set(string(kwd_type),_type);
        d.set(string(kwd_selector),_selector);
        d.set(string(kwd_has_selector),!_selector.empty());
        d.set(string(kwd_card_min),to_string(_card_min));
        d.set(string(kwd_card_max),to_string(_card_max));
        return d;
    }
    Attrib(const Y_Node& node, const AttribMap& attribs)
    {
        HandlerMap<void> hmap
            {
                { kwd_descr, PARSE(descr) },
                { kwd_type, PARSE(type) },
                { kwd_selector, PARSE(selector,attribs) },
            };
        KeySet mandatory { kwd_descr, kwd_type };
        handle_static_map(node, hmap, mandatory);
    }
};

class Structure : public Type
{
    static constexpr string_view _struct_tmpl = R"TMPL(
class {{name}} : public Struct<{{name}}>
{
public:
    constexpr static char _name[] = "{{name}}";
    constexpr static char _descr[] = "{{descr}}";
    constexpr static string_view _codes[] {
        {{#attribs}}
        "{{name}}",
        {{/attribs}}
        };
    constexpr static int _attribcnt = sizeof(_codes)/sizeof(_codes[0]);
    constexpr static array<string_view,_attribcnt> _adescr {
        {{#attribs}}
        "{{descr}}",
        {{/attribs}}
        };

{{#attribs}}
    Attrib<{{type}},{{card_min}},{{card_max}}> {{name}}{{#has_selector}} { {{selector}}.get() }{{/has_selector}};
{{/attribs}}

    const array<BaseAttrib*,_attribcnt> _attribs {
        {{#attribs}}
        &{{name}},
        {{/attribs}}
        };
};
)TMPL";

    AttribMap _attribs;
    void parse_attribs(const Y_Node& node)
    {
        handle_dynamic_map<Attrib*>(node, _attribs, CREATE(Attrib,_attribs));
    }
    mdata get_mdata(string_view name)
    {
        mdata d;
        d.set(string(kwd_name),string(name));
        d.set(string(kwd_descr),_descr);

        mdata attribs {mdata::type::list};
        for (const auto& [k, v] : _attribs) attribs << v->get_mdata(k);
        d.set(string(kwd_attribs),attribs);
        return d;
    }
public:
    void render(string_view name,ostream& os)
    {
        render_tmpl(_struct_tmpl,get_mdata(name),os);
    }
    Structure(const Y_Node& node)
    {
        HandlerMap<void> hmap
            {
                { kwd_descr, PARSE(descr) },
                { kwd_attribs, PARSE(attribs) },
                { kwd_kind, PARSE(kind) },
            };
        KeySet mandatory { kwd_descr, kwd_attribs };
        handle_static_map(node, hmap, mandatory);
    }
};

#endif
