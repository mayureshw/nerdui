#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <mustache.hpp>
#include <ranges>
#include <vector>
#include <map>

using mdata = kainjow::mustache::data;
using kainjow::mustache::partial;
using mustache = kainjow::mustache::mustache;
using namespace std;

constexpr string_view kwd_constants  = "constants";
constexpr string_view kwd_types      = "types";
constexpr string_view kwd_domain     = "domain";
constexpr string_view kwd_union      = "union";
constexpr string_view kwd_structure  = "structure";
constexpr string_view kwd_kind       = "kind";
constexpr string_view kwd_descr      = "descr";
constexpr string_view kwd_values     = "values";

class YamlIf
{
public:
    static constexpr const char* nodeTypeName(YAML::NodeType::value t)
    {
        switch (t) {
        case YAML::NodeType::Map:      return "map";
        case YAML::NodeType::Sequence: return "sequence";
        case YAML::NodeType::Scalar:   return "scalar";
        case YAML::NodeType::Null:     return "null";
        case YAML::NodeType::Undefined:return "undefined";
        }
        return "unknown";
    }
    static void print_err_loc(const YAML::Node& node, char* path)
    {
        cerr << " " << path << ":";
        auto mark = node.Mark();
        if ( mark.is_null() ) cerr << "unknown";
        else cerr << mark.line + 1 << ":" << mark.column + 1;
        cerr << endl;
    }
    template <YAML::NodeType::value Expected>
    static void expectType(const YAML::Node& node,char* path)
    {
        if (node.Type() != Expected)
        {
            cerr << "Expected yaml object of type: " << nodeTypeName(Expected);
            print_err_loc(node,path);
            exit(1);
        }
    }
    static void expectKey(const YAML::Node& node,string_view key,char* path)
    {
        // Assumption: Caller does expectType
        if ( not node[key] )
        {
            auto mark = node.Mark();
            cerr << "Key '" << key << "' missing in map";
            print_err_loc(node,path);
            exit(1);
        }
    }
};

class Type : public YamlIf
{
protected:
    const string _name;
public:
    Type(string name) : _name(name) {}
    virtual ~Type() {}
};

class Constant
{
};

class Domain : public Type
{
    map<string,string> _keyvals;
public:
    Domain(string name, YAML::Node& node, char* path) : Type(name)
    {
        expectKey(node,kwd_values,path);
        auto kvals = node[kwd_values];
        for (const auto& kv : kvals)
        {
            auto key = kv.first.as<string>();
            if ( _keyvals.find(key) != _keyvals.end() )
            {
                cerr << "Duplicate key found " << "'" << key << "'";
                print_err_loc(node,path);
                exit(1);
            }
            expectType<YAML::NodeType::Scalar>(kv.second,path);
            auto val = kv.second.as<string>();
            _keyvals.emplace(key,val);
        }
    }
};

class Union : public Type
{
public:
    Union(string name, YAML::Node& node, char* path) : Type(name)
    {
    }
};

class Structure : public Type
{
public:
    Structure(string name, YAML::Node& node, char* path) : Type(name)
    {
    }
};

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
    void process_type(string name, YAML::Node& node, char* path)
    {
        expectType<YAML::NodeType::Map> (node,path);
        expectKey(node,kwd_kind,path);
        expectKey(node,kwd_descr,path);
        auto kind = node[kwd_kind].as<string>();
        Type *typ;
        if ( kind == kwd_domain ) typ = new Domain(name,node,path);
        else if ( kind == kwd_union ) typ = new Union(name,node,path);
        else if ( kind == kwd_structure ) typ = new Structure(name,node,path);
        else
        {
            cerr << "Unknown kind: '" << kind << "'";
            print_err_loc(node,path);
            exit(1);
        }
        _types.emplace(name,typ);
    }
    void process_types(YAML::Node& node, char* path)
    {
        expectType<YAML::NodeType::Map> (node,path);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            if ( _types.find(key) != _types.end() )
            {
                cerr << "Duplicate type found : '" << key << "'";
                print_err_loc(node,path);
                exit(1);
            }
            auto value = kv.second;
            process_type(key,value,path);
        }
    }
    void process_constant(string key, string value, char* path)
    {
        auto it = _constants.find(key);
        if ( it == _constants.end() ) _constants.emplace(key,value);
        else
        {
            cerr << "Duplicate constant found '" << key << "'"
                << path << " : " << key << endl;
            exit(1);
        }
    }
    void process_constants(YAML::Node& node, char* path)
    {
        expectType<YAML::NodeType::Map> (node,path);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            auto value = kv.second;
            expectType<YAML::NodeType::Scalar>(value,path);
            auto value_s = value.as<string>();
            process_constant(key,value_s,path);
        }
    }
    void process_yaml(YAML::Node& node, char* path)
    {
        expectType<YAML::NodeType::Map> (node,path);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            auto value = kv.second;
            if ( key == kwd_types ) process_types(value,path);
            else if ( key == kwd_constants ) process_constants(value,path);
            else
            {
                cerr << "Unknown key: '" << key << "'";
                print_err_loc(node,path);
                exit(1);
            }
       }
    }
public:
    void parse_yaml(char* path)
    {
        try {
            auto yaml = YAML::LoadFile(path);
            process_yaml(yaml,path);
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
