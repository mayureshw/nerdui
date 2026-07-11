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
    static constexpr string_view _sessions_tmpl = R"TMPL(
#define APP_TITLE "{{app_title}}"

using DefaultSessionType = {{default_session}};
)TMPL";

    static constexpr string_view _head_tmpl = R"TMPL(
#ifndef _GENTYPES_H_
#define _GENTYPES_H_

#include "basetypes.h"

)TMPL";

    static constexpr string_view _tail_tmpl = R"TMPL(
#endif
)TMPL";

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
    void render_tmpl(string_view tmplstr, mdata d, ostream& os)
    {
        mustache tmpl {string(tmplstr)};
        tmpl.render(d,os);
    }
    mdata get_const_mdata()
    {
        mdata d;
        for(const auto& [k,v] : _constants) d.set(k,v);
        return d;
    }
public:
    void render(ostream& os = cout)
    {
        mdata blankd;
        render_tmpl(_head_tmpl,blankd,os);
        for(auto t:_types) t.second->render(os);
        render_tmpl(_sessions_tmpl,get_const_mdata(),os);
        render_tmpl(_tail_tmpl,blankd,os);
    }
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
    yaml_spec.render();
}
