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
    virtual mdata get_mdata(string_view name)
    {
        mdata d;
        return d;
    }
    virtual void render_types_h(string_view name, ostream& os)
    {
        render_tmpl(get_tmpl_types_h(),get_mdata(name),os);
    }
    virtual string_view get_tmpl_types_h()
    {
        return "";
    }
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
    constexpr static inline e_choice_widget _choice_widget = e_choice_widget::{{choice_widget}};
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

    OrderedMap<string,string> _keyvals;
    string _choice_widget { kwd_dropdown };
    void parse_values(const Y_Node& node)
    {
        handle_dynamic_map<string>(node, _keyvals, PARSE(dynamic_string));
    }
    void parse_choice_widget(const Y_Node& node)
    {
        _choice_widget = parse_static_string(node,dom_choice_widget);
    }
public:
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
    string_view get_tmpl_types_h() { return _domain_tmpl; }
    Domain(const Y_Node& node)
    {
        HandlerMap<void> hmap
            {
                { kwd_descr, PARSE(descr) },
                { kwd_values, PARSE(values) },
                { kwd_choice_widget, PARSE(choice_widget) },
                { kwd_kind, PARSE(kind) },
            };
        KeySet mandatory { kwd_values };
        handle_static_map(node, hmap, mandatory);
    }
};

class UnionCases : public YamlIf
{
    OrderedMap<string,string> _keytyp;
    string parse_type(const Y_Node& node)
    {
        auto type = parse_dynamic_string(node);
        check_type_exists(node,type);
        return type;
    }
public:
    mdata get_mdata()
    {
        return map_to_list(_keytyp);
    }
    UnionCases(const Y_Node& node)
    {
        handle_dynamic_map<string>(node, _keytyp, PARSE(type));
    }
};

class Union : public Type
{
    static constexpr string_view _union_tmpl = R"TMPL(
class {{name}} : public Union<{{name}},{{selectorTyp}},{{#cases}}{{#value}}{{value}}{{^is_last}},{{/is_last}}{{/value}}{{^is_last}},{{/is_last}}{{/cases}}>
{
public:
    constexpr static char _name[] = "{{name}}";
    constexpr static char _descr[] = "{{descr}}";
    template <typename Callback> void dispatch(Callback cb)
    {
{{^selectorIsUnion}}
        using t_selectorEnum = e_{{selectorTyp}};
        switch( _selector.val() ) {
{{#cases}}
{{#value}}
        case t_selectorEnum::{{key}}: return cb.template operator()<{{value}}>();
{{/value}}
{{/cases}}
        default: return cb.template operator()<monostate>();
        }
{{/selectorIsUnion}}
{{#selectorIsUnion}}
        visit(overloaded{
{{#cases}}
        [&]({{key}}& curvariant) {
        using t_selectorEnum = e_{{key}};
            switch(curvariant.val()) {
{{#value}}
            case t_selectorEnum::{{key}}: return cb.template operator()<{{value}}>();
{{/value}}
            default: return cb.template operator()<monostate>();
            }
{{#value}}
{{/value}}
        },
{{/cases}}
        [&](auto& curvariant) {
            return cb.template operator()<monostate>();
        },
        }, _selector._u);
{{/selectorIsUnion}}
    }
};
)TMPL";

    string _selectorTyp;
    bool _selectorIsUnion;
    OrderedMap<string,UnionCases*> _typ_cases;
    void parse_selector_typ(const Y_Node& node)
    {
        _selectorTyp = parse_dynamic_string(node);
        auto typ = check_type_exists(node,_selectorTyp);
        _selectorIsUnion = typ->is_union() ;
    }
    void parse_cases(const Y_Node& node)
    {
        handle_dynamic_map<UnionCases*>(node, _typ_cases,
            CREATE(UnionCases), check_type_exists);
    }
public:
    mdata get_mdata(string_view name)
    {
        mdata d;
        d.set(string(kwd_name),string(name));
        d.set(string(kwd_descr),_descr);
        d.set(string(kwd_selector_typ),_selectorTyp);
        d.set(string(kwd_selectorIsUnion),_selectorIsUnion);
        mdata cases {mdata::type::list};
        const auto& v = _typ_cases.as_vec();
        for(auto it=v.begin(); it!=v.end(); it++)
        {
            mdata d;
            d.set(string(kwd_key),(*it)->first);
            d.set(string(kwd_value),(*it)->second->get_mdata());
            d.set(string(kwd_is_last),next(it)==v.end());
            cases << move(d);
        }
        d.set(string(kwd_cases),cases);
        return d;
    }
    string_view get_tmpl_types_h() { return _union_tmpl; }
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
        KeySet mandatory { kwd_selector_typ, kwd_cases };
        handle_static_map(node, hmap, mandatory);
    }
};

class Attrib;
using AttribMap = OrderedMap<string,Attrib*>;

class Attrib : public YamlIf
{
    string _descr;
    string _type;
    bool _type_is_union;
    string _selector;
    string _persistence_type { kwd_transient };
    uint32_t _card_min = 0;
    uint32_t _card_max = 1;
    void parse_descr(const Y_Node& node)
    {
        _descr = parse_dynamic_string(node);
    }
    void parse_type(const Y_Node& node)
    {
        _type = parse_dynamic_string(node);
        _type_is_union = check_type_exists(node,_type)->is_union();
    }
    void parse_selector(const Y_Node& node, const AttribMap& attribs)
    {
        _selector = parse_dynamic_string(node);
        if ( not attribs.contains(_selector) )
        {
            cerr << "Selector used before declaring " << _selector;
            print_err_loc(node);
            exit(1);
        }
    }
    void parse_persistence_type(const Y_Node& node)
    {
        _persistence_type = parse_static_string(node,dom_persistence_type);
    }
    void validate(const Y_Node& node)
    {
        if ( _selector.empty() and _type_is_union )
        {
            cerr << "For attribute of union type, selector attribute must be specified ";
            print_err_loc(node);
            exit(1);
        }
    }
public:
    mdata get_mdata(string_view name, string_view contained_in, size_t ordpos)
    {
        mdata d;
        d.set(string(kwd_contained_in),string(contained_in));
        d.set(string(kwd_ordpos),to_string(ordpos));
        d.set(string(kwd_name),string(name));
        d.set(string(kwd_descr),_descr);
        d.set(string(kwd_type),_type);
        d.set(string(kwd_selector),_selector);
        d.set(string(kwd_has_selector),!_selector.empty());
        d.set(string(kwd_card_min),to_string(_card_min));
        d.set(string(kwd_card_max),to_string(_card_max));
        d.set(string(kwd_persistence_type),_persistence_type);
        return d;
    }
    Attrib(const Y_Node& node, const AttribMap& attribs)
    {
        HandlerMap<void> hmap
            {
                { kwd_descr, PARSE(descr) },
                { kwd_type, PARSE(type) },
                { kwd_selector, PARSE(selector,attribs) },
                { kwd_persistence_type, PARSE(persistence_type) },
            };
        KeySet mandatory { kwd_type };
        handle_static_map(node, hmap, mandatory);
        validate(node);
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
    constexpr static bool _persistent = {{persistent}};
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
    Attrib<{{contained_in}},{{ordpos}},{{type}},{{card_min}},{{card_max}},e_persistence_type::{{persistence_type}}> {{name}}{{#has_selector}} { {{selector}}.get() }{{/has_selector}};
{{/attribs}}
    auto attributes()
    {
        return tie({{#attribs}}{{name}}{{^is_last}},{{/is_last}}{{/attribs}});
    }
};
)TMPL";

    AttribMap _attribs;
    string _persistent { kwd_false };
    void parse_attribs(const Y_Node& node)
    {
        handle_dynamic_map<Attrib*>(node, _attribs, CREATE(Attrib,_attribs));
    }
    void parse_persistent(const Y_Node& node)
    {
        _persistent = parse_static_string(node,dom_bool);
    }
public:
    mdata get_mdata(string_view name)
    {
        mdata d;
        d.set(string(kwd_name),string(name));
        d.set(string(kwd_descr),_descr);
        d.set(string(kwd_persistent),_persistent);

        mdata attribs {mdata::type::list};
        const auto& attrv = _attribs.as_vec();
        size_t ordpos = 0;
        for (const auto& a : attrv)
        {
            auto attrd = a->second->get_mdata(a->first,name,ordpos++);
            attrd.set(string(kwd_is_last),ordpos == attrv.size());
            attribs << attrd;
        }
        d.set(string(kwd_attribs),attribs);
        return d;
    }
    string_view get_tmpl_types_h() { return _struct_tmpl; }
    Structure(const Y_Node& node)
    {
        HandlerMap<void> hmap
            {
                { kwd_descr, PARSE(descr) },
                { kwd_attribs, PARSE(attribs) },
                { kwd_kind, PARSE(kind) },
                { kwd_persistent, PARSE(persistent) },
            };
        KeySet mandatory { kwd_attribs };
        handle_static_map(node, hmap, mandatory);
    }
};

#endif
