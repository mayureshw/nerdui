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

constexpr string_view kwd_constants      = "constants";
constexpr string_view kwd_types          = "types";
constexpr string_view kwd_domain         = "domain";
constexpr string_view kwd_union          = "union";
constexpr string_view kwd_structure      = "structure";
constexpr string_view kwd_kind           = "kind";
constexpr string_view kwd_name           = "name";
constexpr string_view kwd_descr          = "descr";
constexpr string_view kwd_values         = "values";
constexpr string_view kwd_choice_widget  = "choice_widget";
constexpr string_view kwd_selector_typ   = "selectorTyp";
constexpr string_view kwd_selector       = "selector";
constexpr string_view kwd_has_selector   = "has_selector";
constexpr string_view kwd_selectorIsUnion= "selectorIsUnion";
constexpr string_view kwd_DropDown       = "DropDown";
constexpr string_view kwd_Radio          = "Radio";
constexpr string_view kwd_Button         = "Button";
constexpr string_view kwd_attribs        = "attribs";
constexpr string_view kwd_type           = "type";
constexpr string_view kwd_cases          = "cases";
constexpr string_view kwd_key            = "key";
constexpr string_view kwd_value          = "value";
constexpr string_view kwd_card_min       = "card_min";
constexpr string_view kwd_card_max       = "card_max";
constexpr string_view kwd_is_last        = "is_last";

const KeySet emptyKeySet {};
const KeySet dom_choice_widget { kwd_DropDown, kwd_Radio, kwd_Button };

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
