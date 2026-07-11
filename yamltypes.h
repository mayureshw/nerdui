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

class Attrib;
using AttribMap = map<string,Attrib*>;

class Attrib : public YamlIf
{
    string _descr;
    string _type;
    string _selector;
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
    AttribMap _attribs;
    void parse_attribs(const Y_Node& node)
    {
        handle_dynamic_map<Attrib*>(node, _attribs, CREATE(Attrib,_attribs));
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
