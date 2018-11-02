#include "duk_modules.hpp"
#include "scripting_api.hpp"
#include "duk_module_duktape.h"
#include <map>
#include <string>
#include <libncclient/nc_util.hpp>
#include "duk_object_functions.hpp"
#include <iostream>
#include "memory_sandbox.hpp"

std::map<std::string, std::string>& module_cache()
{
    static bool init = false;
    static std::map<std::string, std::string> dat;

    if(!init)
    {
        dat["@babel/polyfill"] = read_file_bin("./script_compile/node_modules/@babel/polyfill/dist/polyfill.min.js");
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

void dukx_push_fixed_buffer(duk_context* ctx, const std::string& buffer)
{
    char *ptr = (char*)duk_push_fixed_buffer(ctx, buffer.size());

    for(int i=0; i < (int)buffer.size(); i++)
    {
        ptr[i] = buffer[i];
    }
}

duk_int_t duk_get_func(duk_context* ctx)
{
    std::string str = duk_safe_to_std_string(ctx, 0);

    if(str == "@babel/polyfill")
    {
        //std::string data = module_cache()["@babel/polyfill"];

        auto bin = module_binary_cache()["@babel/polyfill"];

        dukx_push_fixed_buffer(ctx, bin);
        duk_load_function(ctx);

        duk_pcall(ctx, 0);
        duk_pop(ctx);

        //push_duk_val(ctx, data);

        push_duk_val(ctx, "");
        return 1;
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
}

void init_module_cache()
{
    module_cache();
    module_binary_cache();
}
