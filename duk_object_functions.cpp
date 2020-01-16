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

#if 0
inline
duk_ret_t dukx_proxy_get_prototype_of(duk_context* ctx)
{
    //printf("gproto\n");

    //duk_get_prototype(ctx, 0);

    duk_push_undefined(ctx);

    return 1;
}

inline
duk_ret_t dukx_proxy_set_prototype_of(duk_context* ctx)
{
    //printf("sproto\n");

    //duk_set_prototype(ctx, 0);

    duk_push_false(ctx);

    return 1;
}

///uuh
inline
duk_ret_t dukx_proxy_is_extensible(duk_context* ctx)
{
    //printf("ext\n");

    duk_push_true(ctx);
    return 1;
}

inline
duk_ret_t dukx_proxy_prevent_extension(duk_context* ctx)
{
    //printf("pext\n");

    return 0;
}

inline
duk_ret_t dukx_proxy_get_own_property(duk_context* ctx)
{
    //printf("gprop\n");

    duk_get_prop_desc(ctx, 0, 0);
    return 1;
}

inline
duk_ret_t dukx_proxy_define_property(duk_context* ctx)
{
    //printf("dprop\n");

    duk_push_true(ctx);
    return 1;
}

inline
duk_ret_t dukx_proxy_has(duk_context* ctx)
{
    //printf("hprop\n");

    duk_push_boolean(ctx, duk_has_prop(ctx, 0));
    return 1;
}

inline
duk_ret_t dukx_stringify_parse(duk_context* ctx)
{
    duk_push_current_function(ctx);
    duk_get_prop_string(ctx, -1, DUKX_HIDDEN_SYMBOL("json_me_harder").c_str());
    duk_dup(ctx, -1);
    duk_json_encode(ctx, -1);
    duk_json_decode(ctx, -1);

    return 1;
}

///HEY
///THIS FUNCTION IS AN EXCEPTION
///MUST HANDLE ITS OWN SANITISATION
///AS SOMETIMES IT LETS SLIP SOMETHING THAT ISNT SANITISED ON PURPOSE
inline
duk_ret_t dukx_proxy_get(duk_context* ctx)
{
    //printf("get\n");

    //duk_pop(ctx);

    /*int top = duk_get_top(ctx);

    for(int i=0; i < top; i++)
    {
        duk_dup(ctx, i);

        //printf("stack top: %i\n", duk_get_top(ctx));

        printf("%i val %s\n", i, duk_safe_to_string(ctx, -1));

        duk_pop(ctx);
    }*/


    duk_dup(ctx, 1);

    std::string_view sview = duk_safe_to_string_view(ctx, -1);

    bool is_proto = sview == "__proto__";
    bool is_toJSON = sview == "toJSON";

    duk_pop(ctx);

    if(is_proto)
    {
        return 0;
    }

    ///return target

    ///NO SANITISE PASTH
    if(is_toJSON)
    {
        duk_pop(ctx);
        duk_pop(ctx);

        duk_push_c_function(ctx, dukx_stringify_parse, 0);
        duk_dup(ctx, -2);
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("json_me_harder").c_str());

        //duk_push_true(ctx);
        //duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("no_proxy").c_str());

        /*duk_def_prop(ctx, -1,
             DUK_DEFPROP_HAVE_WRITABLE |
             DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_ENUMERABLE |
             DUK_DEFPROP_HAVE_CONFIGURABLE | DUK_DEFPROP_FORCE);*/

        duk_freeze(ctx, -1);

        //printf("stringify\n");

        //duk_pop(ctx);
        return 1;
    }

    /*if(str == "valueOf")
    {
        duk_pop(ctx);
        duk_pop(ctx);
        ///needs to be a function
        duk_push_object(ctx);
        return 1;
    }*/

    duk_pop(ctx);

    if(!duk_get_prop(ctx, 0))
        duk_push_undefined(ctx);

    dukx_sanitise_in_place(ctx, -1);

    return 1;
}

inline
duk_ret_t dukx_proxy_set(duk_context* ctx)
{
    //printf("set\n");

    ///remove receiver
    duk_remove(ctx, 3);

    duk_dup(ctx, 2);

    if(duk_safe_to_std_string(ctx, -1) == "__proto__")
        throw std::runtime_error("Thou shalt not modify __proto__");

    duk_pop(ctx);

    duk_push_boolean(ctx, duk_put_prop(ctx, 0));

    return 1;
}

inline
duk_ret_t dukx_proxy_delete_property(duk_context* ctx)
{
    //printf("del\n");

    duk_push_boolean(ctx, duk_del_prop(ctx, 0));

    return 1;
}

inline
std::vector<std::string> dukx_get_keys(duk_context* ctx)
{
    std::vector<std::string> keys;

    duk_enum(ctx, -1, 0);
    while(duk_next(ctx, -1, 0))
    {
        keys.push_back(duk_safe_to_std_string(ctx, -1));
        duk_pop(ctx);
    }

    //std::cout << "fnum keys " << keys.size() << std::endl;

    duk_pop(ctx);

    return keys;
}

inline
void dukx_hack_in_keys(duk_context* ctx, duk_idx_t idx, const std::vector<std::string>& keys)
{
    for(auto& i : keys)
    {
        duk_push_number(ctx, 0.f);
        duk_put_prop_string(ctx, -1 + idx, i.c_str());
    }
}

duk_ret_t dukx_proxy_apply(duk_context* ctx);

inline
duk_ret_t dukx_proxy_construct(duk_context* ctx)
{
    //printf("pcst\n");

    duk_push_true(ctx);

    return 1;
}

inline
duk_ret_t dukx_dummy(duk_context* ctx)
{
    return 0;
}

inline
duk_ret_t dukx_proxy_own_keys(duk_context* ctx)
{
    auto keys = dukx_get_keys(ctx);
    push_duk_val(ctx, keys);
    return 1;

    /*duk_push_array(ctx);

    duk_enum(ctx, -2, DUK_ENUM_NO_PROXY_BEHAVIOR);

    int idx = 0;

    while(duk_next(ctx, -1, 0))
    {
        duk_put_prop_index(ctx, -3, idx++);
        duk_pop(ctx);
    }

    duk_pop(ctx);

    return 1;*/
}

template<duk_c_function t>
inline
duk_ret_t dukx_wrap_ctx(duk_context* ctx)
{
    ///args are on the stack, top is number of args
    int top = duk_get_top(ctx);

    ///[...args, function]
    duk_push_current_function(ctx);
    ///[...args, function, internal object]
    duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL("WRAPPED"));
    ///[...args, internal object]
    duk_remove(ctx, -2);

    ///[internal object, ...args1-n]
    //duk_replace(ctx, -1 - top);
    duk_replace(ctx, -1 - top);

    duk_push_c_lightfunc(ctx, t, top, top, 0);

    ///[func, internal object, ...args1-n]
    duk_insert(ctx, -1 - top);

    duk_int_t rc = duk_pcall(ctx, top);

    ///value on stack
    if(t != dukx_proxy_get && t != dukx_proxy_own_keys)
        dukx_sanitise_in_place(ctx, -1);
    else
    {
        ///do nothing
    }

    if(rc != DUK_EXEC_SUCCESS)
        return duk_throw(ctx);

    return 1;

    #if 0
    #define OLD_GLOBAL_SWAP
    #ifdef OLD_GLOBAL_SWAP
    ///[arg1, arg2... argtop]
    int top = duk_get_top(ctx);

    ///[argstop, thread]
    duk_push_thread(ctx);
    //duk_push_thread_new_globalenv(ctx);

    duk_context* new_ctx = duk_get_context(ctx, -1);

    duk_push_object(new_ctx);
    duk_set_global_object(new_ctx);

    ///[thread, argstop]
    duk_insert(ctx, 0);

    duk_require_stack(new_ctx, top+1);

    duk_push_current_function(ctx);
    duk_xmove_top(new_ctx, ctx, 1);
    duk_get_prop_string(new_ctx, -1, DUK_HIDDEN_SYMBOL("WRAPPED"));
    duk_remove(new_ctx, -2);

    duk_xmove_top(ctx, new_ctx, 1);

    ///[thread, argstop, new_arg]

    ///replaces this to [thread, new_arg, argstopexcept0]
    duk_replace(ctx, -1 - top);

    ///ok so we have [thread, argstop]
    ///we want to replace the first arg with the hidden body
    ///[new -> cfunc]
    duk_push_c_lightfunc(new_ctx, t, top, top, 0);
    ///[old -> thread]
    ///[new -> cfunc, argstop]
    duk_xmove_top(new_ctx, ctx, top);

    ///[new -> return]
    duk_int_t rc = duk_pcall(new_ctx, top);

    ///[new -> empty]
    ///[old -> thread, return]
    //duk_xmove_top(ctx, new_ctx, 1);

    ///get is special cased because
    ///it can let unsanitised values out
    if(t != dukx_proxy_get && t != dukx_proxy_own_keys)
        dukx_sanitise_move_value(new_ctx, ctx, -1);
    else
        duk_xmove_top(ctx, new_ctx, 1);

    ///remove thread
    ///[old -> return]
    duk_remove(ctx, 0);

    if(rc != DUK_EXEC_SUCCESS)
    {
        return duk_throw(ctx);
    }

    return 1;
    #else
    ///[arg1, arg2... argtop]
    int top = duk_get_top(ctx);

    ///save global object
    duk_push_global_object(ctx);
    duk_insert(ctx, -1 - top);

    ///[global, arg1, arg2... argtop]
    duk_push_object(ctx);
    ///[global, arg1, arg2... argtop, new_global] -> [global, arg1, arg2... argtop]
    duk_set_global_object(ctx);

    duk_push_current_function(ctx);
    duk_get_prop_string(ctx, -1, DUKX_HIDDEN_SYMBOL("WRAPPED").c_str());
    duk_remove(ctx, -2);

    ///we have [old_global, args, new_arg]

    ///replaces this to [old_global, new_arg, argstopexcept0]
    duk_replace(ctx, -1 - top);

    ///takes us to [old_global, new_arg, argstopexcept0, function]
    duk_push_c_function(ctx, t, top);

    ///takes us to [old_global, function, new_arg, argstopexcept0]
    duk_insert(ctx, -1 - top);

    ///[old_global, return]
    duk_int_t rc = duk_pcall(ctx, top);

    ///get is special cased because
    ///it can let unsanitised values out
    if(t != dukx_proxy_get && t != dukx_proxy_own_keys)
        dukx_sanitise_in_place(ctx, -1);
    else
    {
        ///do nothing, unsanitised value is already on stack
    }

    ///duk stack is now [old_global, value]
    duk_dup(ctx, 0);
    duk_set_global_object(ctx);
    duk_remove(ctx, 0);

    if(rc != DUK_EXEC_SUCCESS)
    {
        return duk_throw(ctx);
    }

    return 1;

    #endif // OLD_GLOBAL_SWAP
    #endif // 0
}
///its sanitise in place thats slow

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
    //std::vector<std::string> rkeys = dukx_get_keys(ctx);

    if(duk_is_function(ctx, idx))
        duk_push_c_function(ctx, dukx_dummy, 0);
    else if(duk_is_object(ctx, idx))
        duk_push_object(ctx);
    else
        assert(false);

    ///duk_proxy_ownkeys_postprocess
    //dukx_hack_in_keys(ctx, -1, rkeys);

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

    if(duk_has_prop_string(dst_ctx, idx, DUK_HIDDEN_SYMBOL("PROX")))
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

    duk_push_true(dst_ctx);
    duk_put_prop_string(dst_ctx, -2, DUK_HIDDEN_SYMBOL("PROX"));

    ///[to_wrap, proxy]
    duk_remove(dst_ctx, -1 + idx);

    ///[proxy] left on stack
}

void dukx_sanitise_move_value(duk_context* ctx, duk_context* dst_ctx, duk_idx_t idx)
{
    //printf("top 1 %i top 2 %i\n", duk_get_top(ctx), duk_get_top(dst_ctx));

    if(ctx == dst_ctx)
    {
        ///??????
        ///could disable this for a perf boost
        ///but changes semantics depending on whether or not the global is reused or not
        ///which is... yeah
        ///that said, if the proxy can exactly replicate not the proxy, that may be fine
        dukx_sanitise_in_place(dst_ctx, idx);
        return;
    }

    duk_dup(ctx, idx);
    duk_xmove_top(dst_ctx, ctx, 1);
    duk_remove(ctx, idx);

    dukx_sanitise_in_place(dst_ctx, -1);
}
#endif

std::string get_original_host(duk_context* ctx, duk_idx_t idx)
{
    duk_get_prop_string(ctx, idx, DUKX_HIDDEN_SYMBOL("OHOST").c_str());

    std::string ocaller = duk_safe_to_std_string(ctx, -1);

    duk_pop_n(ctx, 1);

    return ocaller;
}

std::string get_original_host(js::value& val)
{
    return val.get_hidden("OHOST");
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

//void dukx_push_db_proxy(duk_context* ctx);

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
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(*vctx));
        mongo_ctx.change_collection(secret_host);

        found = mongo_ctx->find_json_new(nlohmann::json({}), nlohmann::json());
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
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(*vctx));
        mongo_ctx.change_collection(secret_host);

        {
            std::lock_guard guard(mongo_ctx->backend.get_lock_for());

            std::vector<nlohmann::json>& direct_data = mongo_ctx->backend.get_db_data_nolock_import();

            delete_from_request(mongo_ctx->backend, direct_data, proxy_chain);
        }
    }

    return js::make_success(*vctx);
}

js::value dukx_db_finish_proxy_r(js::value& func, js::value& object);

js::value db_get(js::value_context* vctx, js::value target, js::value prop, js::value val, js::value receiver)
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
            ret = db_set<false>;

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

    js::add_setter(global, "$db", db_set<false>).add_hidden("OHOST", secret_host);
    js::add_getter(global, "$db", js::function<db_getter_get>).add_hidden("OHOST", secret_host);
}

/*void dukx_setup_db_proxy(js::value_context& vctx)
{
    std::string host = get_script_host(vctx);

    js::value global = js::get_global(vctx);

    js::add_setter(global, "$db", db_set<false>).add_hidden("OHOST", host);
    js::add_getter(global, "$db", js::function<db_getter_get>).add_hidden("OHOST", host);
}*/

std::pair<js::value, js::value> dukx_db_push_proxy_handlers(js::value_context& vctx)
{
    js::value dummy_func = js::make_value(vctx, js::function<js::empty_function>);
    js::value dummy_obj(vctx);

    return {dummy_func, dummy_obj};
}

js::value dukx_db_finish_proxy_r(js::value& func, js::value& object)
{
    object.get("get") = js::function<db_get>;
    object.get("set") = db_set<true>;
    object.get("apply") = js::function<db_apply>;

    return js::make_proxy(func, object);
}
