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
    virtual ~Type() {}
};

class Domain : public Type
{
    map<string,string> _keyvals;
    string _choice_widget = "";
    void parse_values(const Y_Node& node)
    {
        handle_dynamic_map<string>(node, _keyvals, PARSE(dynamic_string));
    }
    void parse_choice_widget(const Y_Node& node)
    {
        _choice_widget = parse_static_string(node,dom_choice_widget);
    }
public:
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

class Union : public Type
{
    string _selectorTyp;
public:
    Union(const Y_Node& node)
    {
        // expectKey(node,kwd_selector_typ);
        // auto selectorTypNode = node[kwd_selector_typ];
        // expectType<Y_Scalar>(selectorTypNode);
        // _selectorTyp = selectorTypNode.as<string>();
    }
};

class Attrib
{
public:
    Attrib(const Y_Node& node)
    {
    }
};

class Structure : public Type
{
    map<string,Attrib*> _attribs;
    void parse_attribs(const Y_Node& node)
    {
        handle_dynamic_map<Attrib*>(node, _attribs, CREATE(Attrib));
    }
public:
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
