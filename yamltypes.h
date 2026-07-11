#ifndef _YAML_TYPES_H_
#define _YAML_TYPES_H_

class Type : public YamlIf
{
protected:
    const string _descr;
public:
    virtual ~Type() {}
};

class Domain : public Type
{
    map<string,string> _keyvals;
    string _choice_widget = "";
public:
    Domain(const Y_Node& node)
    {
        // expectKey(node,kwd_values);
        // auto values = node[kwd_values];
        // handle_dynamic_map<string>(values, _keyvals,
        //     [this](const Y_Node& n){ return get_string(n); }
        //     );

        // auto choice_widget = node[kwd_choice_widget];
        // if ( choice_widget ) _choice_widget = choice_widget.as<string>();
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

class Structure : public Type
{
public:
    Structure(const Y_Node& node)
    {
    }
};

#endif
