#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

#include "script_util.hpp"
#include "mongo.hpp"
#include "privileged_core_scripts.hpp"

inline
void quick_register(duk_context* ctx, const std::string& key, const std::string& value)
{
    duk_push_string(ctx, value.c_str());
    duk_put_prop_string(ctx, -2, key.c_str());
}

///#db.i, r, f, u, u1, us,
static
duk_ret_t db_insert(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx->change_collection(get_script_host(ctx));

    std::string json = duk_json_encode(ctx, -1);

    mongo_ctx->insert_json_1(get_script_host(ctx), json);

    //std::cout << "json " << json << std::endl;

    return 1;
}

static
duk_ret_t db_update(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx->change_collection(get_script_host(ctx));

    std::string json_1 = duk_json_encode(ctx, 0);
    std::string json_2 = duk_json_encode(ctx, 1);

    mongo_ctx->update_json_many(get_script_host(ctx), json_1, json_2);

    //std::cout << "update " << json_1 << " with " << json_2 << std::endl;

    return 0;
}

inline
void parse_push_json(duk_context* ctx, const std::vector<std::string>& jsons)
{
    duk_idx_t arr_idx = duk_push_array(ctx);

    for(int i=0; i < (int)jsons.size(); i++)
    {
        duk_push_string(ctx, jsons[i].c_str());
        duk_json_decode(ctx, -1);

        duk_put_prop_index(ctx, arr_idx, i);
    }
}

static
duk_ret_t db_find_all(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx->change_collection(get_script_host(ctx));

    duk_push_this(ctx);

    duk_get_prop_string(ctx, -1, "JSON");
    std::string json = duk_get_string(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "PROJ");
    std::string proj = duk_get_string(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "DB_CALLER");
    std::string caller = duk_get_string(ctx, -1);
    duk_pop(ctx);

    //std::cout << "json " << json << std::endl;

    ///remove get prop db info
    duk_pop(ctx);

    if(caller != get_caller(ctx))
        return 0;

    std::vector<std::string> db_data = mongo_ctx->find_json(get_script_host(ctx), json, proj);

    parse_push_json(ctx, db_data);

    return 1;
}

static
duk_ret_t db_find_one(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx->change_collection(get_script_host(ctx));

    duk_push_this(ctx);

    duk_get_prop_string(ctx, -1, "JSON");
    std::string json = duk_get_string(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "PROJ");
    std::string proj = duk_get_string(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "DB_CALLER");
    std::string caller = duk_get_string(ctx, -1);
    duk_pop(ctx);

    //std::cout << "json " << json << std::endl;

    ///remove get prop db info
    duk_pop(ctx);

    if(caller != get_caller(ctx))
        return 0;

    std::vector<std::string> db_data = mongo_ctx->find_json(get_script_host(ctx), json, proj);

    if(db_data.size() == 0)
    {
        duk_push_undefined(ctx);
    }
    else
    {
        duk_push_string(ctx, db_data[0].c_str());
        duk_json_decode(ctx, -1);
    }

    return 1;
}

///count, first, array

///note to self: Attach ids to object
///then simply freeze, none of this nonsense
static
duk_ret_t db_find(duk_context* ctx)
{
    COOPERATE_KILL();

    int nargs = duk_get_top(ctx);

    std::string json = "";//duk_json_encode(ctx, -1);
    std::string proj = "";

    if(nargs == 2)
    {
        json = duk_json_encode(ctx, 0);
        proj = std::string("{ \"projection\" : ") + duk_json_encode(ctx, 1) + " }";
    }

    if(nargs == 1)
    {
        json = duk_json_encode(ctx, 0);
    }

    if(nargs == 0 || nargs > 2)
        return 0;

    duk_push_object(ctx);

    duk_push_string(ctx, json.c_str());
    duk_put_prop_string(ctx, -2, "JSON");

    duk_push_string(ctx, proj.c_str());
    duk_put_prop_string(ctx, -2, "PROJ");

    duk_push_string(ctx, get_caller(ctx).c_str());
    duk_put_prop_string(ctx, -2, "DB_CALLER");

    //[object]
    duk_push_c_function(ctx, db_find_all, 0);
    duk_put_prop_string(ctx, -2, "array");

    duk_push_c_function(ctx, db_find_one, 0);
    duk_put_prop_string(ctx, -2, "first");

    duk_freeze(ctx, -1);

    return 1;
}

static
duk_ret_t db_remove(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx->change_collection(get_script_host(ctx));

    std::string json = duk_json_encode(ctx, -1);

    mongo_ctx->remove_json(get_script_host(ctx), json);

    return 0;
}

inline
void startup_state(duk_context* ctx, const std::string& caller, const std::string& script_host, const std::string& script_ending)
{
    duk_push_global_stash(ctx);

    quick_register(ctx, "HASH_D", "");
    quick_register(ctx, "caller", caller.c_str());
    quick_register(ctx, "script_host", script_host.c_str());
    quick_register(ctx, "script_ending", script_ending.c_str());

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "DB_ID");

    duk_pop_n(ctx, 1);
}

static
duk_ret_t hash_d(duk_context* ctx)
{
    COOPERATE_KILL();

    std::string str = duk_json_encode(ctx, -1);

    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "HASH_D");

    std::string fstr = duk_safe_to_string(ctx, -1);

    fstr += str + "\n";

    duk_pop_n(ctx, 1);

    duk_push_string(ctx, fstr.c_str());
    duk_put_prop_string(ctx, -2, "HASH_D");

    duk_pop_n(ctx, 2);

    return 0;
}

///needs to be moved somewhere better
inline
std::string get_hash_d(duk_context* ctx)
{
    COOPERATE_KILL();

    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "HASH_D");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

inline
std::string compile_and_call(stack_duk& sd, const std::string& data, std::string caller, bool stringify, int seclevel)
{
    if(data.size() == 0)
    {
        duk_push_undefined(sd.ctx);

        return "Script not found";
    }

    register_funcs(sd.ctx, seclevel);

    std::string wrapper = attach_wrapper(data, stringify, false);

    //std::cout << wrapper << std::endl;

    std::string ret;

    duk_push_string(sd.ctx, wrapper.c_str());
    duk_push_string(sd.ctx, "test-name");

    //DUK_COMPILE_FUNCTION
    if(duk_pcompile(sd.ctx, DUK_COMPILE_FUNCTION | DUK_COMPILE_STRICT) != 0)
    {
        ret = duk_safe_to_string(sd.ctx, -1);

        printf("compile failed: %s\n", ret.c_str());

        duk_push_undefined(sd.ctx);
    }
    else
    {
        duk_push_global_stash(sd.ctx);
        duk_push_int(sd.ctx, seclevel);
        duk_put_prop_string(sd.ctx, -2, "last_seclevel");
        duk_pop_n(sd.ctx, 1);

        duk_push_global_object(sd.ctx); //[glob]

        duk_idx_t id = duk_push_object(sd.ctx); ///context //[glob -> obj]
        duk_push_string(sd.ctx, caller.c_str()); ///caller //[glob -> obj -> string]
        duk_put_prop_string(sd.ctx, id, "caller"); //[glob -> obj]

        duk_put_prop_string(sd.ctx, -2, "context"); //[glob]

        duk_pop_n(sd.ctx, 1); //empty stack, has function at -1

        duk_get_global_string(sd.ctx, "context"); //[context]

        int nargs = 2;

        if(duk_is_undefined(sd.ctx, -3))
        {
            nargs = 1;
        }
        else
        {
            duk_dup(sd.ctx, -3); //[args]
        }

        duk_pcall(sd.ctx, nargs);
    }

    std::string str = get_hash_d(sd.ctx);

    ///only should do this if the caller is owner of script
    if(str != "")
    {
        ret = str;
    }

    return ret;
}

static
duk_ret_t js_call(duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string str;

    duk_push_current_function(ctx);

    if(!get_duk_keyvalue(ctx, "FUNCTION_NAME", str))
    {
        duk_pop(ctx);

        duk_push_undefined(ctx);

        push_error(ctx, "Bad script name, this is the developer scolding you, you know what you did");
        return 1;
    }

    duk_pop(ctx);

    if(!is_valid_full_name_string(str))
        return push_error(ctx, "Bad script name, don't do this :)");

    ///current script
    std::string full_script = get_script_host(ctx) + "." + get_script_ending(ctx);

    std::string conv = str;

    ///IF IS PRIVILEGED SCRIPT, RETURN THAT CFUNC
    if(privileged_functions.find(conv) != privileged_functions.end())
    {
        SL_GUARD(privileged_functions[conv].sec_level);

        ///use ORIGINAL script host
        priv_context priv_ctx(get_script_host(ctx));

        set_script_info(ctx, str);

        duk_ret_t result = privileged_functions[conv].func(priv_ctx, ctx, sl);

        //std::string to_return = duk_json_encode(ctx, -1);

        set_script_info(ctx, full_script);

        return result;
    }

    script_info script;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

        //script.load_from_disk_with_db_metadata(str);
        script.name = str;
        script.load_from_db(mongo_ctx);
    }

    if(!script.valid)
    {

        {
            user current_user;

            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

                current_user.load_from_db(mongo_ctx, get_caller(ctx));
            }

            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

            std::string unparsed_source = current_user.get_loaded_callable_scriptname_source(item_ctx, str);

            if(unparsed_source == "")
                return push_error(ctx, "Script not found");

            duk_context* temp_context = js_interop_startup();
            register_funcs(temp_context, 0);

            script_info script_2;
            std::string compile_err = script_2.load_from_unparsed_source(temp_context, unparsed_source, str);

            js_interop_shutdown(temp_context);

            if(compile_err != "")
                return push_error(ctx, "Script Bundle Error: " + compile_err);

            script = script_2;
        }

        if(!script.valid)
            return push_error(ctx, "Script not found");

        //push_error(ctx, "Script not found");
        //return 1;
    }

    SL_GUARD(script.seclevel);

    std::string load = script.parsed_source;

    //std::cout << load << std::endl;

    stack_duk sd;
    sd.ctx = ctx;

    set_script_info(ctx, str);

    compile_and_call(sd, load, get_caller(ctx), false, script.seclevel);

    set_script_info(ctx, full_script);

    return 1;
}

inline
std::string js_unified_force_call_data(duk_context* ctx, const std::string& data, const std::string& host)
{
    set_script_info(ctx, host + ".invoke");

    stack_duk sd;
    sd.ctx = ctx;

    script_info dummy;
    dummy.load_from_unparsed_source(ctx, attach_wrapper(data, false, true), host + ".invoke");

    if(!dummy.valid)
        return "Invalid Command Line Syntax";

    set_global_int(ctx, "last_seclevel", dummy.seclevel);

    duk_push_undefined(ctx);

    compile_and_call(sd, dummy.parsed_source, get_caller(ctx), false, dummy.seclevel);

    if(!duk_is_object_coercible(ctx, -1))
        return "No return";

    //std::string ret = duk_json_encode(ctx, -1);

    std::string ret;

    if(duk_is_string(ctx, -1))
        ret = duk_safe_to_std_string(ctx, -1);
    else
    {
        const char* fnd = duk_json_encode(ctx, -1);

        if(fnd != nullptr)
            ret = std::string(fnd);
        else
            ret = "Bad Output, could not be JSON'd";
    }

    duk_pop(ctx);

    return ret;
}

template<int N>
static
duk_ret_t jxs_call(duk_context* ctx)
{
    int current_seclevel = get_global_int(ctx, "last_seclevel");

    duk_ret_t ret = js_call(ctx, N);

    set_global_int(ctx, "last_seclevel", current_seclevel);

    register_funcs(ctx, current_seclevel);

    return ret;
}

static
duk_ret_t err(duk_context* ctx)
{
    push_error(ctx, "Scriptor syntax is the same as function call syntax, do not use .call");
    return 1;
}

///so ideally this would provide validation
///pass through context and set appropriately
///and modify args
template<int N>
inline
duk_ret_t sl_call(duk_context* ctx)
{
    static_assert(N >= 0 && N <= 4);

    std::string str = duk_require_string(ctx, -1);

    duk_push_c_function(ctx, &jxs_call<N>, 1);

    put_duk_keyvalue(ctx, "FUNCTION_NAME", str);
    put_duk_keyvalue(ctx, "call", err);

    freeze_duk(ctx);

    return 1;
}

inline
std::string add_freeze(const std::string& name)
{
    return " global." + name + " = deepFreeze(global." + name + ");\n";
}

inline
void do_freeze(duk_context* ctx, const std::string& name, std::string& script_accumulate)
{
    duk_push_global_object(ctx);

    duk_push_string(ctx, name.c_str());

    duk_def_prop(ctx, -2,
             DUK_DEFPROP_HAVE_WRITABLE |
             DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_ENUMERABLE |
             DUK_DEFPROP_HAVE_CONFIGURABLE);

    duk_pop(ctx);

    script_accumulate += add_freeze(name);
}

inline
void fully_freeze_recurse(duk_context* ctx, std::string& accum){}

template<typename U, typename... T>
inline
void fully_freeze_recurse(duk_context* ctx, std::string& accum, const U& arg, T&&... args)
{
    do_freeze(ctx, arg, accum);

    fully_freeze_recurse(ctx, accum, args...);
}

template<typename... T>
inline
void fully_freeze(duk_context* ctx, T&&... args)
{
    std::string get_global = "var global = new Function(\'return this;\')();";

    std::string freeze_script = get_global + "\nfunction deepFreeze(o) {\n"
          "Object.freeze(o);\n"

          "Object.getOwnPropertyNames(o).forEach(function(prop) {\n"
            "if (o.hasOwnProperty(prop)\n"
            "&& o[prop] !== null\n"
            "&& (typeof o[prop] === \"object\" || typeof o[prop] === \"function\")\n"
            "&& !Object.isFrozen(o[prop])) {\n"
                "deepFreeze(o[prop]);\n"
              "}\n"
          "});\n"

         "return o;\n"
        "};\n\n";

    fully_freeze_recurse(ctx, freeze_script, args...);

    duk_int_t res = duk_peval_string(ctx, freeze_script.c_str());

    if(res != 0)
    {
        std::string err = duk_safe_to_string(ctx, -1);

        printf("freeze eval failed: %s\n", err.c_str());
    }
    else
    {
        duk_pop(ctx);
    }
}

inline
void remove_func(duk_context* ctx, const std::string& name)
{
    duk_push_global_object(ctx);

    duk_push_undefined(ctx);
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop(ctx);
}

inline
void register_funcs(duk_context* ctx, int seclevel)
{
    remove_func(ctx, "fs_call");
    remove_func(ctx, "hs_call");
    remove_func(ctx, "ms_call");
    remove_func(ctx, "ls_call");
    remove_func(ctx, "ns_call");

    if(seclevel <= 4)
        inject_c_function(ctx, sl_call<4>, "fs_call", 1);

    if(seclevel <= 3)
        inject_c_function(ctx, sl_call<3>, "hs_call", 1);

    if(seclevel <= 2)
        inject_c_function(ctx, sl_call<2>, "ms_call", 1);

    if(seclevel <= 1)
        inject_c_function(ctx, sl_call<1>, "ls_call", 1);

    if(seclevel <= 0)
        inject_c_function(ctx, sl_call<0>, "ns_call", 1);

    inject_c_function(ctx, hash_d, "hash_d", 1);

    inject_c_function(ctx, db_insert, "db_insert", 1);
    inject_c_function(ctx, db_find, "db_find", DUK_VARARGS);
    inject_c_function(ctx, db_remove, "db_remove", 1);
    inject_c_function(ctx, db_update, "db_update", 2);

    //fully_freeze(ctx, "hash_d", "db_insert", "db_find", "db_remove", "db_update");
}

#endif // SECCALLERS_HPP_INCLUDED
