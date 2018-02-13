#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

static
duk_ret_t js_call(duk_context* ctx)
{
    printf("js\n");

    return 0;
}

inline
duk_ret_t sl_call(duk_context* ctx, int sl)
{
    printf("SL CALL %i\n", sl);

    duk_push_c_function(ctx, &js_call, 2);

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
