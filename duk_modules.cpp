#include "duk_modules.hpp"
#include "scripting_api.hpp"
#include "duk_module_duktape.h"
#include <map>
#include <string>
#include <libncclient/nc_util.hpp>
#include "duk_object_functions.hpp"
#include <iostream>
#include "memory_sandbox.hpp"
#include <SFML/Graphics.hpp>
#include <tinydir/tinydir.h>
#include "directory_helpers.hpp"

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

            ///(function(require,exports,module){

            dat[lookup] = data;
            dat[name] = data;

            //std::cout << "lookup " << lookup << std::endl;
        });

        init = true;
    }

    return dat;
}

std::map<std::string, std::string>& module_binary_cache()
{
    static bool init = false;
    static std::map<std::string, std::string> saved_map;

    if(!init)
    {
        auto cache = module_cache();

        duk_context* ctx = create_sandbox_heap();

        for(auto& data : cache)
        {
            duk_push_string(ctx, data.second.c_str());
            duk_push_string(ctx, data.first.c_str());

            duk_pcompile(ctx, DUK_COMPILE_EVAL);

            duk_int_t ret = duk_pcall(ctx, 0);

            if(ret != DUK_EXEC_SUCCESS)
            {
                throw std::runtime_error("Bad module binary cache");
            }

            duk_dump_function(ctx);

            duk_size_t out;
            char* ptr = (char*)duk_get_buffer(ctx, -1, &out);

            std::string buf(ptr, out);

            saved_map[data.first] = buf;

            duk_pop(ctx);
        }

        duk_destroy_heap(ctx);

        init = true;
    }

    return saved_map;
}

duk_int_t duk_get_func(duk_context* ctx)
{
    std::string str = duk_safe_to_std_string(ctx, 0);

    auto cache = module_binary_cache();
    auto uncached = module_cache();

    ///might as well go ham

    if(str == "@babel/polyfill" || str == "es6" || str == "es6+" || str == "esnext" || str == "p" || str == "poly" || str == "polyfill")
    {
        //std::string data = module_cache()["@babel/polyfill"];

        auto bin = cache["@babel/polyfill"];

        //sf::Clock clk;

        dukx_push_fixed_buffer(ctx, bin);
        duk_load_function(ctx);

        //std::cout << "clk time " << clk.restart().asMilliseconds() << std::endl;

        ///pcall is slow
        duk_pcall(ctx, 0);
        duk_pop(ctx);

        //std::cout << "clk time2 " << clk.restart().asMilliseconds() << std::endl;

        //push_duk_val(ctx, data);


        push_duk_val(ctx, wrap(""));
        return 1;
    }
    else if(uncached.find(str) != uncached.end())
    {
        auto bin = uncached[str];

        //printf("ran %s\n", str.c_str());

        /*auto bin = cache[str];

        //std::cout << "eval " << str << std::endl;

        dukx_push_fixed_buffer(ctx, bin);
        duk_load_function(ctx);

        duk_dup(ctx, 2);
        duk_dup(ctx, 1);
        duk_dup(ctx, 2);
        duk_dup(ctx, 3);

        //int res = duk_pcall(ctx, 0);

        int res = duk_pcall_method(ctx, 3);

        if(res != DUK_EXEC_SUCCESS)
        {
            std::string err = duk_safe_to_std_string(ctx, -1);

            std::cout << "errrr " << err << std::endl;
        }

        //duk_pop(ctx);

        duk_get_prop_string(ctx, 3, "exports");*/

        push_duk_val(ctx, bin);
        return 1;
    }
    else
    {
        std::cout << "did not find " << str << std::endl;
    }

    /*if(str == "hello")
    {
        duk_push_string(ctx, "var global = new Function('return this')(); global.hello = function () { print(\"Hello world from bar!\"); };");
        duk_push_string(ctx, "test-name");

        duk_pcompile(ctx, DUK_COMPILE_EVAL);
        //duk_pcall(ctx, 0);

        duk_dup(ctx, -1);

        duk_dump_function(ctx);

        duk_size_t out;
        char* ptr = (char*)duk_get_buffer(ctx, -1, &out);

        std::string buf(ptr, out);

        duk_pop(ctx);
        duk_pop(ctx);

        dukx_push_fixed_buffer(ctx, buf);

        duk_load_function(ctx);

        duk_pcall(ctx, 0);
        duk_pop(ctx);

        push_duk_val(ctx, "");
        return 1;
    }*/

    duk_throw(ctx);
    return 1;
}

void dukx_inject_modules(duk_context* ctx)
{
    duk_module_duktape_init(ctx);

    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Duktape");

    duk_push_c_function(ctx, duk_get_func, 4);
    duk_put_prop_string(ctx, -2, "modSearch");
    duk_pop(ctx);
    //duk_pop(ctx);
}

void init_module_cache()
{
    module_cache();
    module_binary_cache();
}
