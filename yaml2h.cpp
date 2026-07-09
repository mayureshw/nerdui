#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <mustache.hpp>
#include <ranges>
#include <vector>

using mdata = kainjow::mustache::data;
using kainjow::mustache::partial;
using mustache = kainjow::mustache::mustache;
using namespace std;

constexpr string_view kwd_domains    = "domains";
constexpr string_view kwd_unions     = "unions";
constexpr string_view kwd_structures = "structures";
constexpr string_view kwd_constants  = "constants";

class Type
{
};

class Constant
{
};

class Domain : public Type
{
};

class Union : public Type
{
};

class Structure : public Type
{
};

class YamlSpec
{
    void check_file_exists(char *path)
    {
        if ( not filesystem::exists(path) )
        {
            cerr << "File not found: " << path << endl;
            exit(1);
        }
    }
    constexpr const char* nodeTypeName(YAML::NodeType::value t)
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
    template <YAML::NodeType::value Expected>
    void expectType(const YAML::Node& node,char* path)
    {
        if (node.Type() != Expected)
        {
            auto mark = node.Mark();
            cerr << "Expected yaml object of type: " << nodeTypeName(Expected)
                << " : " << path << ":";
            if ( mark.is_null() ) cerr << "unknown";
            else cerr << mark.line + 1 << ":" << mark.column + 1;
            cerr << endl;
            exit(1);
        }
    }
    void process_domains(YAML::Node& node, char* path)
    {
    }
    void process_unions(YAML::Node& node, char* path)
    {
    }
    void process_structures(YAML::Node& node, char* path)
    {
    }
    void process_constants(YAML::Node& node, char* path)
    {
        expectType<YAML::NodeType::Map> (node,path);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            auto value = kv.second;
        }
    }
    void process_yaml(YAML::Node& node, char* path)
    {
        expectType<YAML::NodeType::Map> (node,path);
        for (const auto& kv : node)
        {
            auto key = kv.first.as<string>();
            auto value = kv.second;
            if ( key == kwd_domains ) process_domains(value,path);
            else if ( key == kwd_unions ) process_unions(value,path);
            else if ( key == kwd_structures ) process_structures(value,path);
            else if ( key == kwd_constants ) process_constants(value,path);
            else
            {
                cerr << "Unknown key: " << path << ": " << key << endl;
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
