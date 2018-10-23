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

void dukx_push_proxy_functions_nhide(duk_context* ctx, duk_idx_t idx)
{

}

///this function is incorrect with its handling of idx, with respect to duk_put_prop_string
template<typename... X>
void dukx_push_proxy_functions(duk_context* ctx, duk_idx_t idx, const duk_c_function& func, int nargs, const std::string& trap, X... x)
{
    duk_push_c_function(ctx, func, nargs);
    DUKX_HIDE_CTX(ctx);
    //DUKX_HIDE_HOST(ctx);
    duk_put_prop_string(ctx, -1 + idx, trap.c_str());

    dukx_push_proxy_functions(ctx, idx, x...);
}

///this function handles idx correctly but does not hide things
template<typename... X>
void dukx_push_proxy_functions_nhide(duk_context* ctx, duk_idx_t idx, const duk_c_function& func, int nargs, const std::string& trap, X... x)
{
    duk_push_c_function(ctx, func, nargs);
    duk_put_prop_string(ctx, -1 + idx, trap.c_str());

    dukx_push_proxy_functions_nhide(ctx, idx, x...);
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

//void dukx_push_db_proxy(duk_context* ctx);

void dukx_db_push_proxy_handlers(duk_context* ctx);
void dukx_db_finish_proxy(duk_context* ctx);

///todo: multilevel, store current lookup chain as a hidden property
///ok. so db_get basically checks if the result is a primitive
///if it is, we return the result directly, else let slip another proxy

///ok, redesign here
///proxy get only stores the string to be looked up from the db
///then when .fetch() is called, we actually perform the lookup

bool is_number(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), isdigit);
}

std::vector<std::string> normalise_object_stack(const std::string& chain)
{
    std::string proxy_chain = chain;

    while(proxy_chain.size() > 0 && proxy_chain.front() == '.')
    {
        proxy_chain.erase(proxy_chain.begin());
    }

    std::vector<std::string> in = no_ss_split(proxy_chain, ".");

    if(in.size() == 0)
        return in;

    std::string first = in.front();

    if(!is_number(first))
    {
        in.insert(in.begin(), "0");
    }

    return in;
}

nlohmann::json chain_to_request(const std::string& chain)
{
    std::vector<std::string> object_stack = normalise_object_stack(chain);

    nlohmann::json js;

    nlohmann::json exists;
    exists["$exists"] = true;

    std::reference_wrapper<nlohmann::json> last_js = js;

    for(int i=0; i < (int)object_stack.size(); i++)
    {
        last_js.get()[object_stack[i]];

        last_js = last_js.get()[object_stack[i]];
    }

    //std::cout << "ostack stize " << object_stack.size() << std::endl;

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
    std::vector<std::string> object_stack = normalise_object_stack(chain);

    std::reference_wrapper<nlohmann::json> last_js = in;

    for(int i=0; i < (int)object_stack.size(); i++)
    {
        std::string key = object_stack[i];

        if(last_js.get().is_array())
        {
            int val = 0;

            try
            {
                val = std::stoi(key);
            }
            catch(...)
            {
                throw std::runtime_error("Error converting key \"" + key + "\" to integer for array lookup");
            }

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

std::tuple<std::reference_wrapper<nlohmann::json>, std::reference_wrapper<nlohmann::json>, int> get_last_js(std::vector<nlohmann::json>& js, const std::string& chain, nlohmann::json& dummy)
{
    std::reference_wrapper<nlohmann::json> last_js = dummy;
    std::reference_wrapper<nlohmann::json> parent = dummy;
    int collection_root = -1;

    std::vector<std::string> object_stack = normalise_object_stack(chain);

    for(int i=0; i < (int)object_stack.size(); i++)
    {
        std::string key = object_stack[i];

        if(key == "_cid")
        {
            throw std::runtime_error("No setting _cid");
        }

        ///i == 0 is dealing with the implementation detail that the root isn't an object
        ///can't convert because its a direct reference
        if(i == 0)
        {
            int val = 0;

            try
            {
                val = std::stoi(key);
            }
            catch(...)
            {
                throw std::runtime_error("Error converting key \"" + key + "\" to integer for array lookup");
            }

            if(val < 0 || val > (int)js.size())
            {
                throw std::runtime_error("Bad key for root db object access \"" + key + "\", either < 0 or > db.fetch().length (sparse arrays are not supported on the root object!)");
            }

            if(val >= 0 && val < (int)js.size())
            {
                last_js = js[val];
            }

            if(val == (int)js.size())
            {
                nlohmann::json new_coll;
                new_coll[CID_STRING] = db_storage_backend::get_unique_id();

                js.push_back(new_coll);

                last_js = js.back();
            }

            collection_root = val;
        }
        else
        {
            if(last_js.get().is_array())
            {
                int val = 0;

                try
                {
                    val = std::stoi(key);
                }
                catch(...)
                {
                    throw std::runtime_error("Error converting key \"" + key + "\" to integer for array lookup");
                }

                parent = last_js.get();
                last_js = last_js.get()[val];
            }
            else
            {
                last_js.get()[key];

                parent = last_js.get();
                last_js = last_js.get()[key];
            }
        }
    }

    return {last_js, parent, collection_root};
}

///the reason why this is a vector is implementation details unfortunately
void set_from_request(db_storage_backend& ctx, std::vector<nlohmann::json>& js, const std::string& chain, nlohmann::json to_set)
{
    std::vector<std::string> object_stack = normalise_object_stack(chain);

    nlohmann::json dummy;

    auto [last_js, parent, collection_root] = get_last_js(js, chain, dummy);

    if(object_stack.size() == 0)
    {
        ///harvest old CIDs so we can delete them from the disk
        if(!to_set.is_object())
        {
            throw std::runtime_error("value assigned to db or db[index] must be object");
        }

        std::vector<size_t> old_cids;

        for(auto& i : js)
        {
            old_cids.push_back(i.at(CID_STRING).get<size_t>());
        }

        js.clear();

        nlohmann::json new_dat = to_set;
        new_dat[CID_STRING] = db_storage_backend::get_unique_id();

        js.push_back(new_dat);
        ///need to flush collection to disk and delete old db info in that order

        for(auto& i : old_cids)
        {
            nlohmann::json to_erase;
            to_erase[CID_STRING] = i;

            ctx.disk_erase(to_erase);
        }

        ctx.flush(new_dat);
    }
    else
    {
        if(collection_root < 0 || collection_root >= (int)js.size())
            throw std::runtime_error("Bad collection root? " + std::to_string(collection_root));

        size_t old_cid = -1;
        bool has_cid = false;

        //if(last_js.get().count(CID_STRING) > 0)
        if(js[collection_root].count(CID_STRING) > 0)
        {
            old_cid = js[collection_root].at(CID_STRING);
            has_cid = true;
        }

        if(&last_js.get() == &js[collection_root])
        {
            if(!to_set.is_object())
            {
                throw std::runtime_error("value assigned to db or db[index] must be object");
            }
        }

        last_js.get() = to_set;

        if(has_cid)
        {
            js[collection_root][CID_STRING] = old_cid;
        }

        ctx.flush(js[collection_root]);
    }
}

///the reason why this is a vector is implementation details unfortunately
void delete_from_request(db_storage_backend& ctx, std::vector<nlohmann::json>& js, const std::string& chain)
{
    std::vector<std::string> object_stack = normalise_object_stack(chain);

    nlohmann::json dummy;

    auto [last_js, parent, collection_root] = get_last_js(js, chain, dummy);

    ///$db.$delete();
    if(object_stack.size() == 0)
    {
        for(auto& i : js)
        {
            ctx.disk_erase(i);
        }

        js.clear();
    }
    else
    {
        if(collection_root < 0 || collection_root >= (int)js.size())
            throw std::runtime_error("Bad collection index " + std::to_string(collection_root));

        size_t old_cid = -1;
        bool has_cid = false;

        if(js[collection_root].count(CID_STRING) > 0)
        {
            old_cid = js[collection_root].at(CID_STRING);
            has_cid = true;
        }

        ///$db[1].$delete()
        ///parent is null
        if(&last_js.get() == &js[collection_root])
        {
            ctx.disk_erase(js[collection_root]);

            js.erase(js.begin() + collection_root);

            return;
        }

        if(&parent.get() == &dummy)
            throw std::runtime_error("Parent is dummy, should never happen");

        nlohmann::json& parent_js = parent.get();

        std::string last_key = object_stack.back();

        ///$db[index].hello.$delete();

        if(!parent_js.is_array())
        {
            parent_js.erase(last_key);
        }
        else
        {
            if(!is_number(last_key))
                throw std::runtime_error("Attempted to index array with string");

            int idx = std::stoi(last_key);

            parent_js.erase(idx);
        }

        if(has_cid)
        {
            js[collection_root][CID_STRING] = old_cid;
        }

        ctx.flush(js[collection_root]);
    }
}

duk_int_t db_fetch(duk_context* ctx)
{
    duk_push_current_function(ctx);

    std::string proxy_chain = get_chain_of(ctx, -1);
    std::string secret_host = get_original_host(ctx, -1);

    if(secret_host != get_script_host(ctx))
    {
        push_error(ctx, "This almost certainly isn't what you want to happen");
        return 1;
    }

    std::vector<nlohmann::json> found;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
        mongo_ctx.change_collection(secret_host);

        found = mongo_ctx->find_json_new(nlohmann::json({}), nlohmann::json());
    }

    /*for(auto& i : found)
    {
        std::cout << i << std::endl;
    }*/

    if(found.size() == 0)
    {
        duk_push_undefined(ctx);
    }
    /*else if(found.size() == 1)
    {
        push_duk_val(ctx, get_from_request(found[0], proxy_chain));
    }*/
    else
    {
        //push_duk_val(ctx, found);
        push_duk_val(ctx, get_from_request(found, proxy_chain));
    }

    return 1;
}

template<bool is_proxy>
duk_int_t db_set(duk_context* ctx)
{
    if(!is_proxy)
        duk_push_current_function(ctx);
    else
        duk_push_this(ctx);

    std::vector<nlohmann::json> found;

    std::string proxy_chain = get_chain_of(ctx, -1);
    std::string secret_host = get_original_host(ctx, -1);

    duk_pop(ctx);

    nlohmann::json to_set_value = dukx_get_as_json(ctx, is_proxy ? 2 : -1);

    if(is_proxy)
    {
        duk_dup(ctx, 1);

        std::string key = duk_safe_to_std_string(ctx, -1);

        duk_pop(ctx);

        proxy_chain += "." + key;
    }

    if(secret_host != get_script_host(ctx))
    {
        push_error(ctx, "This almost certainly isn't what you want to happen");
        return 1;
    }

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
        mongo_ctx.change_collection(secret_host);

        {
            std::lock_guard guard(mongo_ctx->backend.get_lock_for());

            std::vector<nlohmann::json>& direct_data = mongo_ctx->backend.get_db_data_nolock_import();

            ///need to flush db
            set_from_request(mongo_ctx->backend, direct_data, proxy_chain, to_set_value);
        }
    }

    return 0;
}

template<bool is_proxy>
duk_int_t db_delete(duk_context* ctx)
{
    if(!is_proxy)
        duk_push_current_function(ctx);
    else
        duk_push_this(ctx);

    std::vector<nlohmann::json> found;

    std::string proxy_chain = get_chain_of(ctx, -1);
    std::string secret_host = get_original_host(ctx, -1);

    duk_pop(ctx);

    if(is_proxy)
    {
        duk_dup(ctx, 1);

        std::string key = duk_safe_to_std_string(ctx, -1);

        duk_pop(ctx);

        proxy_chain += "." + key;
    }

    if(secret_host != get_script_host(ctx))
    {
        push_error(ctx, "This almost certainly isn't what you want to happen");
        return 1;
    }

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
        mongo_ctx.change_collection(secret_host);

        {
            std::lock_guard guard(mongo_ctx->backend.get_lock_for());

            std::vector<nlohmann::json>& direct_data = mongo_ctx->backend.get_db_data_nolock_import();

            delete_from_request(mongo_ctx->backend, direct_data, proxy_chain);
        }
    }

    return 0;
}

duk_int_t db_get(duk_context* ctx)
{
    duk_dup(ctx, 1);

    std::string key = duk_safe_to_std_string(ctx, -1);

    ///pass chain into dukx_push_db

    duk_pop(ctx);

    duk_push_this(ctx);

    std::string proxy_chain = get_chain_of(ctx, -1);
    std::string secret_host = get_original_host(ctx, -1);

    duk_pop(ctx);

    ///make it so that fetch also returns the proxy, but if we call that result itll do the fetch function?
    ///need to implement $delete
    if(key == "$fetch" || key == "$" || key == "$set" || key == "$delete")
    {
        if(key == "$fetch" || key == "$")
            duk_push_c_function(ctx, db_fetch, 0);

        if(key == "$set")
            duk_push_c_function(ctx, db_set<false>, 1);

        if(key == "$delete")
            duk_push_c_function(ctx, db_delete<false>, 0);

        duk_push_string(ctx, proxy_chain.c_str());
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("CHAIN").c_str());

        duk_push_string(ctx, secret_host.c_str());
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

        //duk_push_string(ctx, key.c_str());
        //duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("LKEY").c_str());
    }
    else
    {
        //dukx_push_db_proxy(ctx);

        dukx_db_push_proxy_handlers(ctx);

        set_chain(ctx, proxy_chain + "." + key, -1);

        duk_push_string(ctx, secret_host.c_str());
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

        dukx_db_finish_proxy(ctx);
    }

    return 1;
}

duk_int_t db_apply(duk_context* ctx)
{
    duk_push_this(ctx);

    std::string proxy_chain = get_chain_of(ctx, -1);
    std::string secret_host = get_original_host(ctx, -1);

    duk_pop(ctx);

    duk_push_c_function(ctx, db_fetch, 0);

    duk_push_string(ctx, proxy_chain.c_str());
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("CHAIN").c_str());

    duk_push_string(ctx, secret_host.c_str());
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

    std::cout << "doing fetch on chain " << proxy_chain << std::endl;

    duk_pcall(ctx, 0);
    return 1;
}

duk_int_t db_getter_get(duk_context* ctx)
{
    duk_push_current_function(ctx);

    std::string secret_host = get_original_host(ctx, -1);

    duk_pop(ctx);

    //dukx_push_db_proxy(ctx);
    dukx_db_push_proxy_handlers(ctx);

    set_chain(ctx, "", -1);

    duk_push_string(ctx, secret_host.c_str());
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

    dukx_db_finish_proxy(ctx);

    return 1;
}

void dukx_setup_db_proxy(duk_context* ctx)
{
    std::string host = get_script_host(ctx);

    duk_push_global_object(ctx);

    duk_push_string(ctx, "$db");

    duk_push_c_function(ctx, db_getter_get, 0);
    duk_push_string(ctx, host.c_str());
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

    duk_push_c_function(ctx, db_set<false>, 1);
    duk_push_string(ctx, host.c_str());
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

    duk_def_prop(ctx,
                 -1 - 3,
                 DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_FORCE);

    duk_pop(ctx);
}

#if 0
void dukx_push_db_proxy(duk_context* ctx)
{
    //duk_push_object(ctx);
    duk_push_c_function(ctx, dukx_dummy, 0);
    duk_push_object(ctx);

    ///https://github.com/svaarala/duktape-wiki/blob/master/PostEs5Features.md#proxy-handlers-traps

    duk_require_stack(ctx, 16);

    dukx_push_proxy_functions_nhide(ctx, -1,
                                        //dukx_proxy_get_prototype_of, 1, "getPrototypeOf",
                                        //dukx_proxy_set_prototype_of, 2, "setPrototypeOf",
                                        //dukx_proxy_is_extensible, 1, "isExtensible",
                                        //dukx_proxy_prevent_extension, 1, "preventExtension",
                                        //dukx_proxy_get_own_property, 2, "getOwnPropertyDescriptor",
                                        //dukx_proxy_define_property, 3, "defineProperty",
                                        //dukx_proxy_has, 2, "has",
                                        db_get, 3, "get",
                                        db_set<true>, 4, "set",
                                        //dukx_proxy_delete_property, 2, "deleteProperty",
                                        //dukx_proxy_own_keys, 1, "ownKeys",
                                        db_apply, 3, "apply"
                                        //dukx_proxy_construct, 2, "construct");
                                        );

    duk_push_proxy(ctx, 0);

    duk_push_string(ctx, get_script_host(ctx).c_str());
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());
}
#endif // 0

void dukx_db_push_proxy_handlers(duk_context* ctx)
{
    duk_push_c_function(ctx, dukx_dummy, 0);
    duk_push_object(ctx);
}

void dukx_db_finish_proxy(duk_context* ctx)
{
    duk_require_stack(ctx, 16);

    dukx_push_proxy_functions_nhide(ctx, -1,
                                        //dukx_proxy_get_prototype_of, 1, "getPrototypeOf",
                                        //dukx_proxy_set_prototype_of, 2, "setPrototypeOf",
                                        //dukx_proxy_is_extensible, 1, "isExtensible",
                                        //dukx_proxy_prevent_extension, 1, "preventExtension",
                                        //dukx_proxy_get_own_property, 2, "getOwnPropertyDescriptor",
                                        //dukx_proxy_define_property, 3, "defineProperty",
                                        //dukx_proxy_has, 2, "has",
                                        db_get, 3, "get",
                                        db_set<true>, 4, "set",
                                        //dukx_proxy_delete_property, 2, "deleteProperty",
                                        //dukx_proxy_own_keys, 1, "ownKeys",
                                        db_apply, 3, "apply"
                                        //dukx_proxy_construct, 2, "construct");
                                        );

    duk_push_proxy(ctx, 0);

    //duk_push_string(ctx, get_script_host(ctx).c_str());
    //duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("OHOST").c_str());
}

void dukx_set_setter(duk_context* ctx, duk_idx_t idx, const std::string& prop, duk_c_function func)
{
    duk_push_string(ctx, prop.c_str());
    duk_push_c_function(ctx, func, 1 /*nargs*/);
    duk_def_prop(ctx,
                 idx - 2,
                 DUK_DEFPROP_HAVE_SETTER);
}

void dukx_set_getter(duk_context* ctx, duk_idx_t idx, const std::string& prop, duk_c_function func)
{
    duk_push_string(ctx, prop.c_str());
    duk_push_c_function(ctx, func, 0 /*nargs*/);
    duk_def_prop(ctx,
                 idx - 2,
                 DUK_DEFPROP_HAVE_GETTER);
}

void dukx_set_getter_setter(duk_context* ctx, duk_idx_t idx, const std::string& prop, duk_c_function getter, duk_c_function setter)
{
    duk_push_string(ctx, prop.c_str());
    duk_push_c_function(ctx, getter, 0 /*nargs*/);
    duk_push_c_function(ctx, setter, 1 /*nargs*/);
    duk_def_prop(ctx,
                 idx - 3,
                 DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER);
}
