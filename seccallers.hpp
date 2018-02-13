#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

inline
void sl_call(duk_context* ctx, int sl)
{

}

static duk_ret_t fs_call(duk_context *ctx)
{
    sl_call(ctx, 4);

    return 0;
}

static duk_ret_t hs_call(duk_context *ctx)
{
    sl_call(ctx, 3);

    return 0;
}

static duk_ret_t ms_call(duk_context *ctx)
{
    sl_call(ctx, 2);

    return 0;
}

static duk_ret_t ls_call(duk_context *ctx)
{
    sl_call(ctx, 1);

    return 0;
}

static duk_ret_t ns_call(duk_context *ctx)
{
    sl_call(ctx, 0);

    return 0;
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
