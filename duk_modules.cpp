#include "duk_modules.hpp"
#include "scripting_api.hpp"
#include "duk_module_duktape.h"
#include <map>
#include <string>
#include <libncclient/nc_util.hpp>
#include "duk_object_functions.hpp"

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

duk_int_t duk_get_func(duk_context* ctx)
{
    std::string str = duk_safe_to_std_string(ctx, 0);

    if(str == "@babel/polyfill")
    {
        std::string data = module_cache()["@babel/polyfill"];

        push_duk_val(ctx, data);
        return 1;
    }

    duk_throw(ctx);
    return 1;
}

void dukx_inject_modules(duk_context* ctx)
{
#ifdef TESTING
    duk_module_duktape_init(ctx);

    auto cache = module_cache();

    #if 0
    duk_eval_string(ctx,
R"D(
Duktape.modSearch = function (id, require, exports, module) {
    if (id === '@babel/polyfill')
    {
        console.log("hello");
        return
    }

    throw new Error('cannot find module: ' + id);
}
  )D");

  /// 'exports.hello = function () { print("Hello world from bar!"); };';
    #endif // 0

    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Duktape");

    duk_push_c_function(ctx, duk_get_func, 4);
    duk_put_prop_string(ctx, -2, "modSearch");
    duk_pop(ctx);


#endif // TESTING
}

void init_module_cache()
{
    module_cache();
}
