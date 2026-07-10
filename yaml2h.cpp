#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <mustache.hpp>
#include <ranges>
#include <vector>
#include <map>
#include <set>

using namespace std;
using mdata = kainjow::mustache::data;
using kainjow::mustache::partial;
using mustache = kainjow::mustache::mustache;
using Y_Node = YAML::Node;
using Y_value = YAML::NodeType::value;
using NodeHandler = function<void(const Y_Node&)>;
template<typename ValTyp>
using ValueHandler = function<ValTyp(const Y_Node&)>;
using HandlerMap = unordered_map<string_view,NodeHandler>;
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
constexpr string_view kwd_descr          = "descr";
constexpr string_view kwd_values         = "values";
constexpr string_view kwd_choice_widget  = "choice_widget";
constexpr string_view kwd_selector_typ   = "selectorTyp";

const KeySet emptyKeySet {};

class YamlIf
{
public:
    static inline char* _curpath;
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
    static void expectKey(const Y_Node& node,string_view key)
    {
        // Assumption: Caller does expectType
        if ( not node[key] )
        {
            auto mark = node.Mark();
            cerr << "Key '" << key << "' missing in map";
            print_err_loc(node);
            exit(1);
        }
    }
    string get_string(const Y_Node& node)
    {
        expectType<Y_Scalar>(node);
        return node.as<string>();
    }
    static void handle_static_map(const Y_Node& node, HandlerMap& hmap,
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
    template<typename ValTyp>
    static void handle_dynamic_map(const Y_Node& node,
        map<string,ValTyp>& tgtmap, ValueHandler<ValTyp> vhandler)
    {
        expectType<Y_Map>(node);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            if ( tgtmap.find(key) != tgtmap.end() )
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
};

class Type : public YamlIf
{
protected:
    const string _descr;
public:
    Type(string descr) : _descr(descr) {}
    virtual ~Type() {}
};

class Domain : public Type
{
    map<string,string> _keyvals;
    string _choice_widget = "";
public:
    Domain(string descr, const Y_Node& node) : Type(descr)
    {
        expectKey(node,kwd_values);
        auto values = node[kwd_values];
        handle_dynamic_map<string>(values, _keyvals,
            [this](const Y_Node& n){ return get_string(n); }
            );

        auto choice_widget = node[kwd_choice_widget];
        if ( choice_widget ) _choice_widget = choice_widget.as<string>();
    }
};

class Union : public Type
{
    string _selectorTyp;
public:
    Union(string descr, const Y_Node& node) : Type(descr)
    {
        expectKey(node,kwd_selector_typ);
        auto selectorTypNode = node[kwd_selector_typ];
        expectType<Y_Scalar>(selectorTypNode);
        _selectorTyp = selectorTypNode.as<string>();
    }
};

class Structure : public Type
{
public:
    Structure(string descr, const Y_Node& node) : Type(descr)
    {
    }
};

#define PARSE(NT) [this](const Y_Node& n){ parse_##NT(n); }
class YamlSpec : public YamlIf
{
    map<string,string> _constants;
    map<string,Type*> _types;
    void check_file_exists(char *path)
    {
        if ( not filesystem::exists(path) )
        {
            cerr << "File not found: " << path << endl;
            exit(1);
        }
    }
    Type* get_type(const Y_Node& node)
    {
        expectType<Y_Map>(node);
        expectKey(node,kwd_kind);
        auto kind = node[kwd_kind].as<string>();
        expectKey(node,kwd_descr);
        auto descr = node[kwd_descr].as<string>();
        Type *typ;
        if ( kind == kwd_domain ) typ = new Domain(descr,node);
        else if ( kind == kwd_union ) typ = new Union(descr,node);
        else if ( kind == kwd_structure ) typ = new Structure(descr,node);
        else
        {
            cerr << "Unknown kind: '" << kind << "'";
            print_err_loc(node);
            exit(1);
        }
        return typ;
    }
    void parse_types(const Y_Node& node)
    {
        handle_dynamic_map<Type*>(node, _types,
            [this](const Y_Node& n){ return get_type(n); }
            );
    }
    void parse_constants(const Y_Node& node)
    {
        handle_dynamic_map<string>(node, _constants,
            [this](const Y_Node& n){ return get_string(n); }
            );
    }
    void parse_top(const Y_Node& node)
    {
        HandlerMap hmap
            {
                { kwd_types, PARSE(types) },
                { kwd_constants, PARSE(constants) },
            };
        handle_static_map(node, hmap);
    }
public:
    void parse_yaml(char* path)
    {
        try {
            auto node = YAML::LoadFile(path);
            _curpath = path;
            parse_top(node);
        }
        catch ( exception& e ) {
            cerr << path << ": " << e.what() << endl;
            exit(1);
        }
    }
    ~YamlSpec()
    {
        for(auto it:_types) delete it.second;
    }
};

int main(int argc, char *argv[])
{
    if ( argc < 1 )
    {
        cerr << "Usage: " << argv[0] << " <yaml-spec>..." << endl;
        exit(1);
    }
    
    YamlSpec yaml_spec;
    for(int i=1; i<argc; i++) yaml_spec.parse_yaml(argv[i]);
}
