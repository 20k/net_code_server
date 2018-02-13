#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

#include "script_util.hpp"

static
duk_ret_t js_call(duk_context* ctx)
{
    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, "TO_CALL_INTERNAL_XXX");

    std::string str = duk_require_string(ctx, -1);

    duk_pop_n(ctx, 2);

    std::string load = parse_script(get_script_from_name_string(base_scripts_string, str));

    //std::cout << load << std::endl;

    stack_duk sd;
    sd.ctx = ctx;

    compile_and_call(sd, load, true);

    ///oh boy
    ///ok, need to read in the file

    return 1;
}

///so ideally this would provide validation
///pass through context and set appropriately
///and modify args
inline
duk_ret_t sl_call(duk_context* ctx, int sl)
{
    std::string str = duk_require_string(ctx, -1);

    duk_push_heap_stash(ctx);
    duk_push_string(ctx, str.c_str());
    duk_put_prop_string(ctx, -2, "TO_CALL_INTERNAL_XXX");

    duk_pop_n(ctx, 2);

    duk_push_c_function(ctx, &js_call, 1);

    return 1;
}

static duk_ret_t fs_call(duk_context *ctx)
{
    return sl_call(ctx, 4);
}

static duk_ret_t hs_call(duk_context *ctx)
{
    return sl_call(ctx, 3);
}

static duk_ret_t ms_call(duk_context *ctx)
{
    return sl_call(ctx, 2);
}

static duk_ret_t ls_call(duk_context *ctx)
{
    return sl_call(ctx, 1);
}

static duk_ret_t ns_call(duk_context *ctx)
{
    return sl_call(ctx, 0);
}

void register_funcs(duk_context* ctx)
{
    inject_c_function(ctx, fs_call, "fs_call", 1);
    inject_c_function(ctx, hs_call, "hs_call", 1);
    inject_c_function(ctx, ms_call, "ms_call", 1);
    inject_c_function(ctx, ls_call, "ls_call", 1);
    inject_c_function(ctx, ns_call, "ns_call", 1);
}

#endif // SECCALLERS_HPP_INCLUDED
