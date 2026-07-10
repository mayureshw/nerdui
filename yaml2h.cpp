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
using Handler = std::function<void(const Y_Node&)>;
using HandlerMap = unordered_map<string_view,Handler>;
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
};

class Type : public YamlIf
{
protected:
    const string _name;
    const string _descr;
public:
    Type(string name, string descr) : _name(name), _descr(descr) {}
    virtual ~Type() {}
};

class Domain : public Type
{
    map<string,string> _keyvals;
    string _choice_widget = "";
public:
    Domain(string name, string descr, const Y_Node& node)
    : Type(name,descr)
    {
        expectKey(node,kwd_values);
        auto kvals = node[kwd_values];
        for (const auto& kv : kvals)
        {
            auto key = kv.first.as<string>();
            if ( _keyvals.find(key) != _keyvals.end() )
            {
                cerr << "Duplicate key found " << "'" << key << "'";
                print_err_loc(node);
                exit(1);
            }
            expectType<Y_Scalar>(kv.second);
            auto val = kv.second.as<string>();
            _keyvals.emplace(key,val);
        }

        auto choice_widget = node[kwd_choice_widget];
        if ( choice_widget ) _choice_widget = choice_widget.as<string>();
    }
};

class Union : public Type
{
    string _selectorTyp;
public:
    Union(string name, string descr, const Y_Node& node)
    : Type(name,descr)
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
    Structure(string name, string descr, const Y_Node& node)
    : Type(name,descr)
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
    void parse_type(string name, const Y_Node& node)
    {
        expectType<Y_Map> (node);
        expectKey(node,kwd_kind);
        auto kind = node[kwd_kind].as<string>();
        expectKey(node,kwd_descr);
        auto descr = node[kwd_descr].as<string>();
        Type *typ;
        if ( kind == kwd_domain ) typ = new Domain(name,descr,node);
        else if ( kind == kwd_union ) typ = new Union(name,descr,node);
        else if ( kind == kwd_structure ) typ = new Structure(name,descr,node);
        else
        {
            cerr << "Unknown kind: '" << kind << "'";
            print_err_loc(node);
            exit(1);
        }
        _types.emplace(name,typ);
    }
    void parse_types(const Y_Node& node)
    {
        expectType<Y_Map> (node);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            if ( _types.find(key) != _types.end() )
            {
                cerr << "Duplicate type found : '" << key << "'";
                print_err_loc(node);
                exit(1);
            }
            auto value = kv.second;
            parse_type(key,value);
        }
    }
    void parse_constant(string key, string value)
    {
        auto it = _constants.find(key);
        if ( it == _constants.end() ) _constants.emplace(key,value);
        else
        {
            cerr << "Duplicate constant found '" << key << "'"
                << _curpath << " : " << key << endl;
            exit(1);
        }
    }
    void parse_constants(const Y_Node& node)
    {
        expectType<Y_Map> (node);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            auto value = kv.second;
            expectType<Y_Scalar>(value);
            auto value_s = value.as<string>();
            parse_constant(key,value_s);
        }
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
