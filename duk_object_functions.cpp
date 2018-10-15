#include "duk_object_functions.hpp"
#include <iostream>
#include "mongo.hpp"
#include <libncclient/nc_util.hpp>

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

#define DUKX_HIDE_HOST(ctx) duk_push_string(ctx, get_script_host(ctx).c_str()); \
                              duk_put_prop_string(ctx, -1 + idx, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

void dukx_make_proxy_base_from(duk_context* ctx, duk_idx_t idx)
{
    std::vector<std::string> rkeys = dukx_get_keys(ctx);

    if(duk_is_function(ctx, idx))
        duk_push_c_function(ctx, dukx_dummy, 0);
    else if(duk_is_object(ctx, idx))
        duk_push_object(ctx);
    else
        assert(false);

    ///duk_proxy_ownkeys_postprocess
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
    //DUKX_HIDE_HOST(ctx);
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

std::string get_original_host(duk_context* ctx, duk_idx_t idx)
{
    duk_get_prop_string(ctx, idx, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

    std::string ocaller = duk_safe_to_std_string(ctx, -1);

    duk_pop_n(ctx, 1);

    return ocaller;
}

std::string get_full_chain(duk_context* ctx)
{
    duk_push_current_function(ctx);
    duk_get_prop_string(ctx, -1, DUKX_HIDDEN_SYMBOL("CHAIN").c_str());

    bool undef = duk_is_undefined(ctx, -1);

    std::string res = duk_safe_to_std_string(ctx, -1);

    if(undef)
        res.clear();

    duk_pop_n(ctx, 2);

    return res;
}

std::string get_chain_of(duk_context* ctx, duk_idx_t idx)
{
    duk_get_prop_string(ctx, idx, DUKX_HIDDEN_SYMBOL("CHAIN").c_str());

    bool undef = duk_is_undefined(ctx, -1);

    std::string res = duk_safe_to_std_string(ctx, -1);

    if(undef)
        res.clear();

    duk_pop_n(ctx, 1);

    return res;
}

void update_chain(duk_context* ctx, const std::string& key, duk_idx_t idx)
{
    std::string current = get_full_chain(ctx);

    ///current may be ""
    std::string next = current + "." + key;

    //duk_push_current_function(ctx);
    duk_push_string(ctx, key.c_str());
    duk_put_prop_string(ctx, idx - 1, DUKX_HIDDEN_SYMBOL("CHAIN").c_str());
}

void set_chain(duk_context* ctx, const std::string& full, duk_idx_t idx)
{
    duk_push_string(ctx, full.c_str());
    duk_put_prop_string(ctx, idx - 1, DUKX_HIDDEN_SYMBOL("CHAIN").c_str());
}

void dukx_push_db_proxy(duk_context* ctx);

///todo: multilevel, store current lookup chain as a hidden property
///ok. so db_get basically checks if the result is a primitive
///if it is, we return the result directly, else let slip another proxy

///ok, redesign here
///proxy get only stores the string to be looked up from the db
///then when .fetch() is called, we actually perform the lookup

nlohmann::json chain_to_request(const std::string& chain)
{
    std::string proxy_chain = chain;

    while(proxy_chain.size() > 0 && proxy_chain.front() == '.')
    {
        proxy_chain.erase(proxy_chain.begin());
    }

    std::vector<std::string> object_stack = no_ss_split(proxy_chain, ".");

    nlohmann::json js;

    nlohmann::json exists;
    exists["$exists"] = true;

    std::reference_wrapper<nlohmann::json> last_js = js;

    for(int i=0; i < (int)object_stack.size(); i++)
    {
        last_js.get()[object_stack[i]];

        last_js = last_js.get()[object_stack[i]];
    }

    std::cout << "ostack stize " << object_stack.size() << std::endl;

    if(object_stack.size() == 0)
    {
        js = nlohmann::json({});
    }
    else
    {
        last_js.get() = exists;
    }

    return js;
}

nlohmann::json get_from_request(nlohmann::json in, const std::string& chain)
{
    std::string proxy_chain = chain;

    while(proxy_chain.size() > 0 && proxy_chain.front() == '.')
    {
        proxy_chain.erase(proxy_chain.begin());
    }

    std::vector<std::string> object_stack = no_ss_split(proxy_chain, ".");

    std::reference_wrapper<nlohmann::json> last_js = in;

    for(int i=0; i < (int)object_stack.size(); i++)
    {
        std::string key = object_stack[i];

        if(last_js.get().is_array())
        {
            int val = std::stoi(key);

            last_js = last_js.get()[val];
        }
        else
        {
            last_js.get()[key];

            last_js = last_js.get()[key];
        }
    }

    return last_js.get();
}

///this is all wrong but on the plus side itll work just fine with the planned implementation everything is resolved
///db.hi is doing hi:{$exists : true}, but its finding objects which contain a key hi
///instead, db.hi should find everything in the db, and then get the key hi, which in the case of an array of objects would give error
///correctly, it'd be db[0].hi
duk_int_t db_fetch(duk_context* ctx)
{
    duk_push_current_function(ctx);

    std::string proxy_chain = get_chain_of(ctx, -1);

    nlohmann::json request = chain_to_request(proxy_chain);

    std::string last_key;

    std::vector<std::string> split_keys = no_ss_split(proxy_chain, ".");

    if(split_keys.size() > 0)
    {
        last_key = split_keys.back();
    }

    std::vector<nlohmann::json> found;

    {
        std::string host = get_original_host(ctx, -1);

        if(host != get_script_host(ctx))
        {
            push_error(ctx, "This almost certainly isn't what you want to happen");
            return 1;
        }

        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
        mongo_ctx.change_collection(host);

        found = mongo_ctx->find_json_new(nlohmann::json({}), nlohmann::json());
    }

    if(found.size() == 0)
    {
        duk_push_undefined(ctx);
    }

    else if(found.size() == 1)
    {
        push_duk_val(ctx, get_from_request(found[0], proxy_chain));
    }
    else
    {
        push_duk_val(ctx, get_from_request(found, proxy_chain));
    }

    return 1;
}

duk_int_t db_get(duk_context* ctx)
{
    duk_dup(ctx, 1);

    std::string key = duk_safe_to_std_string(ctx, -1);

    ///pass chain into dukx_push_db

    duk_pop(ctx);

    std::string proxy_chain = get_chain_of(ctx, 2);

    std::string secret_host = get_original_host(ctx, 2);

    ///make it so that fetch also returns the proxy, but if we call that result itll do the fetch function?
    if(key == "fetch")
    {
        duk_push_c_function(ctx, db_fetch, 0);

        duk_push_string(ctx, proxy_chain.c_str());
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("CHAIN").c_str());

        duk_push_string(ctx, secret_host.c_str());
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

        duk_push_string(ctx, key.c_str());
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("LKEY").c_str());
    }
    else
    {
        dukx_push_db_proxy(ctx);

        set_chain(ctx, proxy_chain + "." + key, -1);
    }

    return 1;
}

void dukx_push_db_proxy(duk_context* ctx)
{
    ///ok key hacking is fixed
    ///so we need basically, get, set, has, and ownKeys or something
    ///get should do db lookup and return a proxy or the val
    ///set should pass through to db
    ///has should check if the key exists?
    ///hmm... fetching everything in the entire db is free...
    ///

    duk_push_object(ctx);
    duk_push_object(ctx);

    ///https://github.com/svaarala/duktape-wiki/blob/master/PostEs5Features.md#proxy-handlers-traps

    duk_require_stack(ctx, 16);

    dukx_push_proxy_functions(ctx, -1,
                                        dukx_proxy_get_prototype_of, 1, "getPrototypeOf",
                                        dukx_proxy_set_prototype_of, 2, "setPrototypeOf",
                                        dukx_proxy_is_extensible, 1, "isExtensible",
                                        dukx_proxy_prevent_extension, 1, "preventExtension",
                                        dukx_proxy_get_own_property, 2, "getOwnPropertyDescriptor",
                                        dukx_proxy_define_property, 3, "defineProperty",
                                        dukx_proxy_has, 2, "has",
                                        db_get, 3, "get",
                                        dukx_proxy_set, 4, "set",
                                        dukx_proxy_delete_property, 2, "deleteProperty",
                                        dukx_proxy_own_keys, 1, "ownKeys",
                                        dukx_proxy_apply, 3, "apply",
                                        dukx_proxy_construct, 2, "construct");

    duk_push_proxy(ctx, 0);

    duk_push_string(ctx, get_script_host(ctx).c_str());
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

    //duk_push_c_function(ctx, db_fetch, 0);
    //duk_put_prop_string(ctx, -2, "fetch");

    ///[to_wrap, proxy]
    //duk_remove(ctx, -1 + -1);
}
