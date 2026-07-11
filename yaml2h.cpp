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

#include "yamlif.h"
#include "yamltypes.h"

class YamlSpec : public YamlIf
{
    map<string,string> _constants;
    map<string,Type*> _types;
    Type* parse_type(const Y_Node& node)
    {
        HandlerMap<Type*> hmap
            {
                { kwd_domain, CREATE(Domain) },
                { kwd_union, CREATE(Union) },
                { kwd_structure, CREATE(Structure) }
            };
        return handle_dispatch<Type*>(node, kwd_kind, hmap);
    }
    void parse_types(const Y_Node& node)
    {
        handle_dynamic_map<Type*>(node, _types, PARSE(type));
    }
    void parse_constants(const Y_Node& node)
    {
        handle_dynamic_map<string>(node, _constants, PARSE(dynamic_string));
    }
    void parse_top(const Y_Node& node)
    {
        HandlerMap<void> hmap
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
