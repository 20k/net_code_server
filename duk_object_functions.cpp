#include "duk_object_functions.hpp"
#include <iostream>
#include "mongo.hpp"
#include <libncclient/nc_util.hpp>
#include "argument_object.hpp"

int32_t get_thread_id(js::value_context& vctx)
{
    return js::get_heap_stash(vctx).get("thread_id");
}

std::string get_caller(js::value_context& vctx)
{
    std::vector<std::string> ret = js::get_heap_stash(vctx).get("caller_stack");

    if(ret.size() == 0)
        return "";

    return ret.back();
}

std::vector<std::string> get_caller_stack(js::value_context& vctx)
{
    return js::get_heap_stash(vctx).get("caller_stack");
}

std::string get_script_host(js::value_context& vctx)
{
    return js::get_heap_stash(vctx).get("script_host");
}

std::string get_script_ending(js::value_context& vctx)
{
    return js::get_heap_stash(vctx).get("script_ending");
}

std::string get_base_caller(js::value_context& vctx)
{
    return js::get_heap_stash(vctx).get("base_caller");
}

std::string get_original_host(js::value& val)
{
    return val.get_hidden("OHOST");
}

std::string get_chain_of(js::value& arg)
{
    if(arg.is_undefined())
        return "";

    return arg.get_hidden("CHAIN");
}

void set_chain(js::value& val, const std::string& full)
{
    val.add_hidden("CHAIN", full);
}

std::pair<js::value, js::value> dukx_db_push_proxy_handlers(js::value_context& vctx);
void dukx_db_finish_proxy(js::value& func, js::value& object);

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

    /*if(!is_number(first))
    {
        in.insert(in.begin(), "0");
    }*/

    if(!is_number(first))
    {
        throw std::runtime_error("Root key must be index, eg $db[0], whereas you passed $db[" + first + "]");
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

js::value db_fetch(js::value_context* vctx)
{
    js::value current_func = js::get_current_function(*vctx);

    std::string proxy_chain = get_chain_of(current_func);
    std::string secret_host = get_original_host(current_func);

    if(secret_host != get_script_host(*vctx))
    {
        return js::make_error(*vctx, "This almost certainly isn't what you want to happen");
    }

    std::vector<nlohmann::json> found;

    {
        disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
        disk_ctx.change_collection(secret_host);

        found = disk_ctx->find_json_new(nlohmann::json({}), nlohmann::json());
    }

    if(found.size() == 0)
    {
        return js::make_value(*vctx, js::undefined);
    }
    /*else if(found.size() == 1)
    {
        push_duk_val(ctx, get_from_request(found[0], proxy_chain));
    }*/
    else
    {
        return js::make_value(*vctx, get_from_request(found, proxy_chain));
    }
}

template<bool is_proxy>
js::value db_set(js::value_context* vctx, js::value val, std::string prop)
{
    js::value root(*vctx);

    if(!is_proxy)
        root = js::get_current_function(*vctx);
    else
        root = js::get_this(*vctx);

    std::vector<nlohmann::json> found;

    std::string proxy_chain = get_chain_of(root);
    std::string secret_host = get_original_host(root);

    nlohmann::json to_set_value = nlohmann::json::parse(val.to_json());

    if(is_proxy)
    {
        proxy_chain += "." + prop;
    }

    if(secret_host != get_script_host(*vctx))
    {
        return js::make_error(*vctx, "This almost certainly isn't what you want to happen");
    }

    {
        disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
        disk_ctx.change_collection(secret_host);

        {
            std::lock_guard guard(disk_ctx->backend.get_lock_for());

            std::vector<nlohmann::json>& direct_data = disk_ctx->backend.get_db_data_nolock_import();

            ///need to flush db
            set_from_request(disk_ctx->backend, direct_data, proxy_chain, to_set_value);
        }
    }

    return js::make_success(*vctx);
}

js::value db_proxy_set(js::value_context* vctx, js::value target, js::value prop, js::value val, js::value receiver)
{
    return db_set<true>(vctx, val, prop);
}

js::value db_getter_set(js::value_context* vctx, js::value val)
{
    return db_set<false>(vctx, val, "");
}

js::value db_delete(js::value_context* vctx)
{
    js::value current_function = js::get_current_function(*vctx);

    std::string proxy_chain = get_chain_of(current_function);
    std::string secret_host = get_original_host(current_function);

    if(secret_host != get_script_host(*vctx))
    {
        return js::make_error(*vctx, "This almost certainly isn't what you want to happen");
    }

    {
        disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
        disk_ctx.change_collection(secret_host);

        {
            std::lock_guard guard(disk_ctx->backend.get_lock_for());

            std::vector<nlohmann::json>& direct_data = disk_ctx->backend.get_db_data_nolock_import();

            delete_from_request(disk_ctx->backend, direct_data, proxy_chain);
        }
    }

    return js::make_success(*vctx);
}

js::value dukx_db_finish_proxy_r(js::value& func, js::value& object);

js::value db_get(js::value_context* vctx, js::value target, js::value prop, js::value receiver)
{
    std::string key = prop;

    js::value current_this = js::get_this(*vctx);

    std::string proxy_chain = get_chain_of(current_this);
    std::string secret_host = get_original_host(current_this);

    ///make it so that fetch also returns the proxy, but if we call that result itll do the fetch function?
    ///need to implement $delete
    if(key == "$fetch" || key == "$" || key == "$set" || key == "$delete")
    {
        js::value ret(*vctx);

        if(key == "$fetch" || key == "$")
            ret = js::function<db_fetch>;

        if(key == "$set")
            ret = js::function<db_getter_set>;

        if(key == "$delete")
            ret = js::function<db_delete>;

        ret.add_hidden("CHAIN", proxy_chain);
        ret.add_hidden("OHOST", secret_host);

        return ret;
    }
    else
    {
        //dukx_push_db_proxy(ctx);

        auto [func, obj] = dukx_db_push_proxy_handlers(*vctx);

        set_chain(obj, proxy_chain + "." + key);

        obj.add_hidden("OHOST", secret_host);

        return dukx_db_finish_proxy_r(func, obj);
    }
}

js::value db_apply(js::value_context* vctx)
{
    js::value jthis = js::get_this(*vctx);

    std::string proxy_chain = get_chain_of(jthis);
    std::string secret_host = get_original_host(jthis);

    js::value func = js::make_value(*vctx, js::function<db_fetch>);

    func.add_hidden("CHAIN", proxy_chain);
    func.add_hidden("OHOST", secret_host);

    auto [success, val] = js::call(func);

    return val;
}

js::value db_getter_get(js::value_context* vctx)
{
    js::value current_function = js::get_current_function(*vctx);

    std::string secret_host = get_original_host(current_function);

    auto [func, obj] = dukx_db_push_proxy_handlers(*vctx);

    set_chain(obj, "");

    obj.add_hidden("OHOST", secret_host);

    return dukx_db_finish_proxy_r(func, obj);
}

void dukx_setup_db_proxy(js::value_context& vctx)
{
    std::string secret_host = get_script_host(vctx);
    js::value global = js::get_global(vctx);

    #ifdef USE_DUKTAPE
    js::add_setter(global, "$db", js::function<db_proxy_set>).add_hidden("OHOST", secret_host);
    js::add_getter(global, "$db", js::function<db_getter_get>).add_hidden("OHOST", secret_host);
    #else
    auto [v1, v2] = js::add_getter_setter(global, "$db", js::function<db_getter_get>, js::function<db_proxy_set>);

    v1.add_hidden("OHOST", secret_host);
    v2.add_hidden("OHOST", secret_host);
    #endif // USE_DUKTAPE
}

std::pair<js::value, js::value> dukx_db_push_proxy_handlers(js::value_context& vctx)
{
    js::value dummy_func = js::make_value(vctx, js::function<js::empty_function>);
    js::value dummy_obj(vctx);

    return {dummy_func, dummy_obj};
}

js::value dukx_db_finish_proxy_r(js::value& func, js::value& object)
{
    object.get("get") = js::function<db_get>;
    object.get("set") = js::function<db_proxy_set>;
    object.get("apply") = js::function<db_apply>;

    return js::make_proxy(func, object);
}
