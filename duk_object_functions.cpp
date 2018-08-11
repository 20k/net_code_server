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

void dukx_sanitise_in_place(duk_context* dst_ctx, duk_idx_t idx)
{
    if(duk_is_primitive(dst_ctx, idx))
        return;

    std::vector<std::string> rkeys = dukx_get_keys(dst_ctx);

    if(duk_is_function(dst_ctx, idx))
        duk_push_c_function(dst_ctx, dukx_dummy, 0);
    else if(duk_is_object(dst_ctx, idx))
        duk_push_object(dst_ctx);
    else
        assert(false);

    dukx_hack_in_keys(dst_ctx, -1, rkeys);

    duk_push_object(dst_ctx);  /* handler */

    ///https://github.com/svaarala/duktape-wiki/blob/master/PostEs5Features.md#proxy-handlers-traps

    duk_require_stack(dst_ctx, 16);


    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_get_prototype_of>, 1);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "getPrototypeOf");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_set_prototype_of>, 2);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "setPrototypeOf");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_is_extensible>, 1);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "isExtensible");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_prevent_extension>, 1);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "preventExtension");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_get_own_property>, 2);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "getOwnPropertyDescriptor");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_define_property>, 3);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "defineProperty");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_has>, 2);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "has");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_get>, 3);  /* 'get' trap */
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "get");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_set>, 4);  /* 'set' trap */
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "set");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_delete_property>, 2);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "deleteProperty");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_own_keys>, 1);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "ownKeys");

    ///crashing
    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_apply>, 3);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "apply");

    duk_push_c_function(dst_ctx, dukx_wrap_ctx<dukx_proxy_construct>, 2);
    DUKX_HIDE();
    duk_put_prop_string(dst_ctx, -1 + idx, "construct");

    /*///[to_wrap, proxy_dummy, handler]
    duk_dup(ctx, -3);

    duk_put_prop_string(dst_ctx, -2, DUKX_HIDDEN_SYMBOL("WRAPPED_OBJECT").c_str());*/

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

