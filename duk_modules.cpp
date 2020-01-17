#include "duk_modules.hpp"
#include "duk_module_duktape.h"
#include <map>
#include <string>
#include <libncclient/nc_util.hpp>
#include "duk_object_functions.hpp"
#include <iostream>
#include "memory_sandbox.hpp"
#include <tinydir/tinydir.h>
#include "directory_helpers.hpp"
#include "argument_object.hpp"

template<typename T>
void for_every_file(const std::string& directory, const T& t)
{
    tinydir_autoclose close(directory);

    while(close.dir.has_next)
    {
        tinydir_file file;
        tinydir_readfile(&close.dir, &file);

        if(!file.is_dir)
        {
            std::string file_name(file.name);

            t(file_name);
        }

        tinydir_next(&close.dir);
    }
}

std::string wrap(const std::string& in)
{
    return "(function(require, exports, module){" + in + "\n})";
}

std::map<std::string, std::string>& module_cache()
{
    static bool init = false;
    static std::map<std::string, std::string> dat;

    ///require(\"core-js/modules/es6.string.iterator\");\n\nrequire(\"core-js/modules/es6.array.from\");\n\nrequire(\"core-js/modules/es6.array.for-each\");

    if(!init)
    {
        dat["@babel/polyfill"] = wrap(read_file_bin("./script_compile/node_modules/@babel/polyfill/dist/polyfill.min.js"));

        std::string root = "./script_compile/node_modules/";
        std::string ext = "core-js/modules/";

        for_every_file(root + ext, [&](std::string name)
        {
            if(name.find(".js") == std::string::npos)
                return;

            if(name.size() < 3)
                throw std::runtime_error("Bad .js file " + name);

            std::string data = read_file_bin(root + ext + name);

            ///remove extension
            name.pop_back();
            name.pop_back();
            name.pop_back();

            std::string lookup = ext + name;

            data = wrap(data);

            dat[lookup] = data;
            dat[name] = data;
        });

        init = true;
    }

    return dat;
}

std::map<std::string, std::string> build_module_binary_cache()
{
    std::map<std::string, std::string> saved_map;

    auto cache = module_cache();

    js::value_context vctx;

    for(auto& data : cache)
    {
        auto [success, result] = js::compile(vctx, data.second, data.first);

        auto [success_2, final_func] = js::call(result);

        if(!success || !success_2)
            throw std::runtime_error("Bad module binary cache");

        std::string dumped = js::dump_function(final_func);

        saved_map[data.first] = dumped;
    }

    return saved_map;
}

std::map<std::string, std::string>& module_binary_cache()
{
    static std::map<std::string, std::string> saved_map = build_module_binary_cache();

    return saved_map;
}

void dukx_inject_modules(js::value_context& vctx)
{
    duk_module_duktape_init(vctx.ctx);
}

void init_module_cache()
{
    module_cache();
    module_binary_cache();
}
