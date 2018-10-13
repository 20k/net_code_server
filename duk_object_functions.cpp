#include "duk_object_functions.hpp"
#include <iostream>

duk_ret_t dukx_proxy_apply(duk_context* ctx)
{
    //printf("apply\n");

    int id = 2;

    int length = duk_get_length(ctx, -1);

    duk_require_stack(ctx, length);

    for(int i=0; i < length; i++)
    {
        duk_get_prop_index(ctx, id, i);

        dukx_sanitise_in_place(ctx, -1);
    }

    duk_remove(ctx, 2);

    duk_int_t rc = duk_pcall_method(ctx, length);

    if(rc == DUK_EXEC_SUCCESS)
    {
        return 1;
    }
    else
    {
        return duk_throw(ctx);
    }

    return 1;
}

#define DUKX_HIDE() duk_dup(dst_ctx, -3 + idx);\
                    duk_put_prop_string(dst_ctx, -1 + idx, DUKX_HIDDEN_SYMBOL("WRAPPED").c_str());

#define DUKX_HIDE_CTX(ctx) duk_dup(ctx, -3 + idx);\
                           duk_put_prop_string(ctx, -1 + idx, DUKX_HIDDEN_SYMBOL("WRAPPED").c_str());

void dukx_make_proxy_base_from(duk_context* ctx, duk_idx_t idx)
{
    std::vector<std::string> rkeys = dukx_get_keys(ctx);

    if(duk_is_function(ctx, idx))
        duk_push_c_function(ctx, dukx_dummy, 0);
    else if(duk_is_object(ctx, idx))
        duk_push_object(ctx);
    else
        assert(false);

    dukx_hack_in_keys(ctx, -1, rkeys);

    duk_push_object(ctx);  /* handler */
}

void dukx_push_proxy_functions(duk_context* ctx, duk_idx_t idx)
{

}

template<typename... X>
void dukx_push_proxy_functions(duk_context* ctx, duk_idx_t idx, const duk_c_function& func, int nargs, const std::string& trap, X... x)
{
    duk_push_c_function(ctx, func, nargs);
    DUKX_HIDE_CTX(ctx);
    duk_put_prop_string(ctx, -1 + idx, trap.c_str());

    dukx_push_proxy_functions(ctx, idx, x...);
}

void dukx_sanitise_in_place(duk_context* dst_ctx, duk_idx_t idx)
{
    if(duk_is_primitive(dst_ctx, idx))
        return;

    dukx_make_proxy_base_from(dst_ctx, idx);

    ///https://github.com/svaarala/duktape-wiki/blob/master/PostEs5Features.md#proxy-handlers-traps

    duk_require_stack(dst_ctx, 16);

    dukx_push_proxy_functions(dst_ctx, idx,
                                        dukx_wrap_ctx<dukx_proxy_get_prototype_of>, 1, "getPrototypeOf",
                                        dukx_wrap_ctx<dukx_proxy_set_prototype_of>, 2, "setPrototypeOf",
                                        dukx_wrap_ctx<dukx_proxy_is_extensible>, 1, "isExtensible",
                                        dukx_wrap_ctx<dukx_proxy_prevent_extension>, 1, "preventExtension",
                                        dukx_wrap_ctx<dukx_proxy_get_own_property>, 2, "getOwnPropertyDescriptor",
                                        dukx_wrap_ctx<dukx_proxy_define_property>, 3, "defineProperty",
                                        dukx_wrap_ctx<dukx_proxy_has>, 2, "has",
                                        dukx_wrap_ctx<dukx_proxy_get>, 3, "get",
                                        dukx_wrap_ctx<dukx_proxy_set>, 4, "set",
                                        dukx_wrap_ctx<dukx_proxy_delete_property>, 2, "deleteProperty",
                                        dukx_wrap_ctx<dukx_proxy_own_keys>, 1, "ownKeys",
                                        dukx_wrap_ctx<dukx_proxy_apply>, 3, "apply",
                                        dukx_wrap_ctx<dukx_proxy_construct>, 2, "construct");

    duk_push_proxy(dst_ctx, 0);

    ///[to_wrap, proxy]
    duk_remove(dst_ctx, -1 + idx);

    ///[proxy] left on stack
}

void dukx_sanitise_move_value(duk_context* ctx, duk_context* dst_ctx, duk_idx_t idx)
{
    //printf("top 1 %i top 2 %i\n", duk_get_top(ctx), duk_get_top(dst_ctx));

    duk_dup(ctx, idx);
    duk_xmove_top(dst_ctx, ctx, 1);
    duk_remove(ctx, idx);

    dukx_sanitise_in_place(dst_ctx, -1);
}

void dukx_push_db_proxy(duk_context* ctx)
{

}
