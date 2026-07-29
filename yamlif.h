#ifndef _YAML_IF_H
#define _YAML_IF_H

using namespace std;
using mdata = kainjow::mustache::data;
using kainjow::mustache::partial;
using mustache = kainjow::mustache::mustache;
using Y_Node = YAML::Node;
using Y_value = YAML::NodeType::value;
template<typename ValTyp>
using NodeHandler = function<ValTyp(const Y_Node&)>;
template<typename ValTyp>
using HandlerMap = unordered_map<string_view,NodeHandler<ValTyp>>;
using KeySet = set<string_view>;

constexpr auto Y_Map       = YAML::NodeType::Map;
constexpr auto Y_Sequence  = YAML::NodeType::Sequence;
constexpr auto Y_Scalar    = YAML::NodeType::Scalar;
constexpr auto Y_Null      = YAML::NodeType::Null;
constexpr auto Y_Undefined = YAML::NodeType::Undefined;

// keywords
constexpr string_view
    kwd_constants        = "constants",
    kwd_types            = "types",
    kwd_domain           = "domain",
    kwd_union            = "union",
    kwd_structure        = "structure",
    kwd_kind             = "kind",
    kwd_name             = "name",
    kwd_descr            = "descr",
    kwd_values           = "values",
    kwd_choice_widget    = "choice_widget",
    kwd_selector_typ     = "selectorTyp",
    kwd_selector         = "selector",
    kwd_has_selector     = "has_selector",
    kwd_selectorIsUnion  = "selectorIsUnion",
    kwd_dropdown         = "dropdown",
    kwd_radio            = "radio",
    kwd_button           = "button",
    kwd_attribs          = "attribs",
    kwd_type             = "type",
    kwd_cases            = "cases",
    kwd_key              = "key",
    kwd_value            = "value",
    kwd_card_min         = "card_min",
    kwd_card_max         = "card_max",
    kwd_is_last          = "is_last",
    kwd_persistent       = "persistent",
    kwd_true             = "true",
    kwd_false            = "false",
    kwd_persistence_type = "persistence_type",
    kwd_transient        = "transient",
    kwd_column           = "column",
    kwd_blob             = "blob",
    kwd_String           = "String";

// generated file names
constexpr string_view
    file_types_h = "types.h";

// key sets
const KeySet
    emptyKeySet          { },
    dom_choice_widget    { kwd_dropdown, kwd_radio, kwd_button },
    dom_bool             { kwd_true, kwd_false },
    dom_persistence_type { kwd_transient, kwd_column, kwd_blob };

#define PARSE(NT,...) [this __VA_OPT__(,) __VA_ARGS__](const Y_Node& n){ return parse_##NT(n __VA_OPT__(,) __VA_ARGS__); }
#define CREATE(TYP,...) [this](const Y_Node& n) { return new TYP(n __VA_OPT__(,) __VA_ARGS__); }

class Type;

class YamlIf
{
public:
    static inline char* _curpath;
    static inline OrderedMap<string,string> _constants;
    static inline OrderedMap<string,Type*> _types;
    static void check_file_exists(char *path)
    {
        if ( not filesystem::exists(path) )
        {
            cerr << "File not found: " << path << endl;
            exit(1);
        }
    }
    static ofstream get_ofstream(string_view path)
    {
        ofstream out {string(path)};
        if ( not out )
        {
            cerr << "Could not open file for writing: " << path << endl;
            exit(1);
        }
        return out;
    }
    void render_tmpl(string_view tmplstr, mdata d, ostream& os)
    {
        mustache tmpl {string(tmplstr)};
        tmpl.render(d,os);
    }
    mdata map_to_list(const OrderedMap<string,string>& m)
    {
        mdata list {mdata::type::list};
        const auto& ordv = m.as_vec();
        for(auto it=ordv.begin(); it!=ordv.end(); it++)
        {
            mdata item;
            item.set(string(kwd_key), (*it)->first);
            item.set(string(kwd_value), (*it)->second);
            item.set(string(kwd_is_last), next(it)==ordv.end());
            list << move(item);
        }
        return list;
    }
    static Type* check_type_exists(const Y_Node& node, string type)
    {
        const auto& m = _types.as_map();
        auto it = m.find(type);
        if ( it == m.end() )
        {
            cerr << "Type used before definition " << type;
            print_err_loc(node);
            exit(1);
        }
        return it->second;
    }
    static constexpr const char* nodeTypeName(Y_value t)
    {
        switch (t) {
        case Y_Map:      return "map";
        case Y_Sequence: return "sequence";
        case Y_Scalar:   return "scalar";
        case Y_Null:     return "null";
        case Y_Undefined:return "undefined";
        }
        return "unknown";
    }
    static void print_err_loc(const Y_Node& node)
    {
        cerr << " " << _curpath << ":";
        auto mark = node.Mark();
        if ( mark.is_null() ) cerr << "unknown";
        else cerr << mark.line + 1 << ":" << mark.column + 1;
        cerr << endl;
    }
    template <Y_value Expected>
    static void expectType(const Y_Node& node)
    {
        if (node.Type() != Expected)
        {
            cerr << "Expected yaml object of type: " << nodeTypeName(Expected);
            print_err_loc(node);
            exit(1);
        }
    }
    static const Y_Node& expectKey(const Y_Node& node,string_view key)
    {
        expectType<Y_Map>(node);
        if ( not node[key] )
        {
            auto mark = node.Mark();
            cerr << "Key '" << key << "' missing in map";
            print_err_loc(node);
            exit(1);
        }
        auto& ret = node[key];
        return ret;
    }
    string parse_dynamic_string(const Y_Node& node)
    {
        expectType<Y_Scalar>(node);
        return node.as<string>();
    }
    string_view parse_static_string(const Y_Node& node, const KeySet& domain)
    {
        expectType<Y_Scalar>(node);
        auto str = node.as<string>();
        auto it = domain.find(str);
        if ( it != domain.end() ) return *it;
        cerr << "Domain violation. Got '" << str;
        cerr << " Expect one of:";
        for(auto d:domain) cerr << " " << d;
        print_err_loc(node);
        exit(1);
    }

    // static map handle descends one level, no return value expected from child
    // child handler should have some side effect
    static void handle_static_map(const Y_Node& node, HandlerMap<void>& hmap,
        const KeySet& mandatory_keys = emptyKeySet )
    {
        expectType<Y_Map>(node);

        for(const auto& k : mandatory_keys)
        {
            if ( not node[k] )
            {
                cerr << "Mandatory key not found in map '" << k << "'"; 
                print_err_loc(node);
                exit(1);
            }
        }

        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            auto& value = kv.second;
            auto it = hmap.find(key);
            if ( it != hmap.end() ) it->second(value);
            else
            {
                cerr << "Unknown key: '" << key << "'";
                print_err_loc(node);
                exit(1);
            }
       }
    }

    static void dummyHandler(const Y_Node&,string) {}

    // dynamic map handle expects a uniform return type for all children
    // and builds the return values into a tgtmap against a key
    template<typename ValTyp>
    static void handle_dynamic_map(const Y_Node& node,
        OrderedMap<string,ValTyp>& tgtmap, NodeHandler<ValTyp> vhandler,
        function<void(const Y_Node&,string)> khandler = dummyHandler)
    {
        expectType<Y_Map>(node);
        for (const auto& kv : node)
        {
            auto key_node = kv.first;
            auto key = key_node.as<string>();
            khandler(key_node,key);
            if ( tgtmap.contains(key) )
            {
                cerr << "Duplicate key '" << key << "'";
                print_err_loc(node);
                exit(1);
            }
            auto& value = kv.second;
            auto tgtvalue = vhandler(value);
            tgtmap.emplace(key,tgtvalue);
        }
    }

    // dispatcher doesn't descend the tree, it merely routes a node
    // and returns the value. Return type ValTyp is expected to be same
    // for all entries in the map
    template<typename ValTyp>
    static ValTyp handle_dispatch(const Y_Node& node,
        string_view dispatch_key, HandlerMap<ValTyp>& hmap)
    {
        expectType<Y_Map>(node);
        auto& dispatch_val_node = expectKey(node,dispatch_key);
        expectType<Y_Scalar>(dispatch_val_node);
        auto dispatch_val = dispatch_val_node.as<string>();
        auto it = hmap.find(dispatch_val);
        if ( it!= hmap.end() ) return it->second(node);
        cerr << "Domain violation. Got '" << dispatch_val;
        cerr << " Expect one of:";
        for(auto d:hmap) cerr << " " << d.first;
        print_err_loc(node);
        exit(1);
    }
};

#endif
