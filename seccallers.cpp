#include "seccallers.hpp"
#include "mongo.hpp"
#include "memory_sandbox.hpp"
#include "rate_limiting.hpp"
#include "unified_scripts.hpp"
#include "privileged_core_scripts.hpp"
#include "shared_duk_worker_state.hpp"

int my_timeout_check(void* udata)
{
    COOPERATE_KILL_UDATA(udata);

    return 0;
}

duk_ret_t native_print(duk_context *ctx)
{
	COOPERATE_KILL();

    std::string str = duk_safe_to_std_string(ctx, -1);

    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "print_str");

    std::string fstr = duk_safe_to_std_string(ctx, -1);

    fstr += str;

    duk_pop_n(ctx, 1);

    duk_push_string(ctx, fstr.c_str());
    duk_put_prop_string(ctx, -2, "print_str");

    duk_pop_n(ctx, 2);

	return 0;
}

duk_ret_t timeout_yield(duk_context* ctx)
{
    COOPERATE_KILL();

    return 0;
}

duk_ret_t db_insert(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(get_script_host(ctx));

    std::string json = duk_json_encode(ctx, -1);

    mongo_ctx->insert_json_1(get_script_host(ctx), json);

    //std::cout << "json " << json << std::endl;

    return 1;
}

duk_ret_t db_update(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(get_script_host(ctx));

    std::string json_1 = duk_json_encode(ctx, 0);
    std::string json_2 = duk_json_encode(ctx, 1);

    mongo_ctx->update_json_many(get_script_host(ctx), json_1, json_2);

    //std::cout << "update " << json_1 << " with " << json_2 << std::endl;

    return 0;
}

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

duk_ret_t db_find_all(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(get_script_host(ctx));

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

duk_ret_t db_find_one(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(get_script_host(ctx));

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


duk_ret_t db_remove(duk_context* ctx)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(get_script_host(ctx));

    std::string json = duk_json_encode(ctx, -1);

    mongo_ctx->remove_json(get_script_host(ctx), json);

    return 0;
}

duk_ret_t set_is_realtime_script(duk_context* ctx)
{
    COOPERATE_KILL();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    shared_state->set_realtime();

    return 0;
}

duk_ret_t async_pipe(duk_context* ctx)
{
    COOPERATE_KILL();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    std::string str = duk_safe_to_std_string(ctx, -1);

    if(str.size() > 100*100)
        str.resize(100*100);

    shared_state->set_output_data(str);

    return 0;
}

duk_ret_t is_realtime_script(duk_context* ctx)
{
    COOPERATE_KILL();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    duk_push_boolean(ctx, shared_state->is_realtime());

    return 1;
}

duk_ret_t set_close_window_on_exit(duk_context* ctx)
{
    COOPERATE_KILL();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    shared_state->set_close_window_on_exit();

    return 0;
}

duk_ret_t set_start_window_size(duk_context* ctx)
{
    COOPERATE_KILL();

    if(duk_is_undefined(ctx, -1) || !duk_is_object_coercible(ctx, -1))
        return push_error(ctx, "Usage: set_start_window_size({width:10, height:25});");

    if(!duk_has_prop_string(ctx, -1, "width") || !duk_has_prop_string(ctx, -1, "height"))
        return push_error(ctx, "Must have width *and* height property");

    int width = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "width", 10.);
    int height = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "height", 10.);

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    shared_state->set_width_height(width, height);

    return 0;
}

duk_ret_t is_key_down(duk_context* ctx)
{
    COOPERATE_KILL();

    if(!duk_is_string(ctx, -1))
    {
        push_duk_val(ctx, false);
        return false;
    }

    std::string str = duk_safe_to_std_string(ctx, -1);

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    bool is_down = shared_state->is_key_down(str);

    push_duk_val(ctx, is_down);

    return 1;
}

duk_ret_t mouse_get_position(duk_context* ctx)
{
    COOPERATE_KILL();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    vec2f dim = shared_state->get_mouse_pos();

    dim = clamp(dim, 0.f, 200.f);

    push_dukobject(ctx, "x", dim.x(), "y", dim.y());

    return 1;
}

void startup_state(duk_context* ctx, const std::string& caller, const std::string& script_host, const std::string& script_ending, const std::vector<std::string>& caller_stack, shared_duk_worker_state* shared_state)
{
    duk_push_global_stash(ctx);

    quick_register(ctx, "HASH_D", "");
    quick_register(ctx, "print_str", "");
    quick_register(ctx, "caller", caller.c_str());
    quick_register_generic(ctx, "caller_stack", caller_stack);
    quick_register(ctx, "script_host", script_host.c_str());
    quick_register(ctx, "script_ending", script_ending.c_str());

    {
        std::lock_guard guard(shim_lock);

        set_copy_allocate_shim_pointer(ctx, c_shim_map);
    }

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "DB_ID");

    dukx_put_pointer(ctx, shared_state, "shared_caller_state");

    duk_pop_n(ctx, 1);
}

void teardown_state(duk_context* ctx)
{
    shared_duk_worker_state* shared_state = dukx_get_pointer<shared_duk_worker_state>(ctx, "shared_caller_state");

    delete shared_state;

    free_shim_pointer<shim_map_t>(ctx);
}

duk_ret_t terminate_realtime(duk_context* ctx)
{
    shared_duk_worker_state* shared_state = dukx_get_pointer<shared_duk_worker_state>(ctx, "shared_caller_state");

    shared_state->disable_realtime();

    return 0;
}

duk_ret_t hash_d(duk_context* ctx)
{
    COOPERATE_KILL();

    if(get_caller(ctx) != get_script_host(ctx))
    {
        return 0;
    }

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

std::string get_hash_d(duk_context* ctx)
{
    duk_push_global_stash(ctx);

    if(!duk_has_prop_string(ctx, -1, "HASH_D"))
    {
        duk_pop(ctx);
        return "";
    }

    duk_get_prop_string(ctx, -1, "HASH_D");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

std::string get_print_str(duk_context* ctx)
{
    duk_push_global_stash(ctx);

    if(!duk_has_prop_string(ctx, -1, "print_str"))
    {
        duk_pop(ctx);
        return "";
    }

    duk_get_prop_string(ctx, -1, "print_str");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

std::string compile_and_call(stack_duk& sd, const std::string& data, std::string caller, bool stringify, int seclevel, bool is_top_level)
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
        std::string err = duk_safe_to_string(sd.ctx, -1);

        printf("compile failed: %s\n", err.c_str());

        duk_push_string(sd.ctx, "Syntax or Compile Error");
    }
    else
    {
        duk_push_global_stash(sd.ctx);
        duk_push_int(sd.ctx, seclevel);
        duk_put_prop_string(sd.ctx, -2, "last_seclevel");
        duk_pop_n(sd.ctx, 1);

        duk_idx_t id = duk_push_object(sd.ctx); ///[object]
        duk_push_string(sd.ctx, caller.c_str()); ///[object -> caller]
        duk_put_prop_string(sd.ctx, id, "caller"); ///[object]

        std::string script_host = get_script_host(sd.ctx);

        duk_push_string(sd.ctx, script_host.c_str());
        duk_put_prop_string(sd.ctx, id, "script_host");

        int nargs = 2;

        if(duk_is_undefined(sd.ctx, -3))
        {
            nargs = 1;
        }
        else
        {
            duk_dup(sd.ctx, -3); //[args]
        }

        duk_int_t ret_val = duk_pcall(sd.ctx, nargs);

        bool timeout = is_script_timeout(sd.ctx);

        if(ret_val != DUK_EXEC_SUCCESS && !timeout)
        {
            std::string err = duk_safe_to_std_string(sd.ctx, -1);

            push_dukobject(sd.ctx, "ok", false, "msg", err);
        }

        if(!is_top_level)
        {
            duk_context* ctx = sd.ctx;

            ///this essentially rethrows an exception
            ///if we're not top level, and we've timedout
            COOPERATE_KILL();
        }

        if(ret_val != DUK_EXEC_SUCCESS && is_top_level && timeout)
        {
            push_dukobject(sd.ctx, "ok", false, "msg", "Ran for longer than 5000ms and timed out");
        }
    }

    std::string str = get_hash_d(sd.ctx);

    ///only should do this if the caller is owner of script
    if(str != "" && is_top_level)
    {
        duk_pop(sd.ctx);

        push_duk_val(sd.ctx, str);
    }

    std::string extra = get_print_str(sd.ctx);

    return extra;
}

duk_ret_t js_call(duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string to_call_fullname;

    duk_push_current_function(ctx);

    if(!get_duk_keyvalue(ctx, "FUNCTION_NAME", to_call_fullname))
    {
        duk_pop(ctx);

        duk_push_undefined(ctx);

        push_error(ctx, "Bad script name, this is the developer scolding you, you know what you did");
        return 1;
    }

    duk_pop(ctx);

    if(!is_valid_full_name_string(to_call_fullname))
        return push_error(ctx, "Bad script name, don't do this :)");

    ///current script
    std::string full_script = get_script_host(ctx) + "." + get_script_ending(ctx);

    #ifdef USE_LOCS
    {
        user usr;

        {
            mongo_lock_proxy user_info = get_global_mongo_user_info_context(get_thread_id(ctx));

            ///so eg, we do #i20k.user_port, we need to load their db and check their user_port
            usr.load_from_db(user_info, get_host_from_fullname(to_call_fullname));
        }

        std::string user_port = usr.user_port;

        if(get_host_from_fullname(to_call_fullname) + "." + user_port == to_call_fullname)
        {
            SL_GUARD(user_port_descriptor.sec_level);

            ///use ORIGINAL script host
            priv_context priv_ctx(get_script_host(ctx), to_call_fullname);

            set_script_info(ctx, to_call_fullname);

            duk_ret_t result = user_port_descriptor.func(priv_ctx, ctx, sl);

            set_script_info(ctx, full_script);

            return result;
        }
    }
    #endif // USE_LOCS

    ///IF IS PRIVILEGED SCRIPT, RETURN THAT CFUNC
    if(privileged_functions.find(to_call_fullname) != privileged_functions.end())
    {
        SL_GUARD(privileged_functions[to_call_fullname].sec_level);

        ///use ORIGINAL script host
        priv_context priv_ctx(get_script_host(ctx), to_call_fullname);

        set_script_info(ctx, to_call_fullname);

        duk_ret_t result = privileged_functions[to_call_fullname].func(priv_ctx, ctx, sl);

        set_script_info(ctx, full_script);

        return result;
    }

    ///so, this indent and everything under if(!script.valid)
    ///needs to be chained into one general "get_script" method

    std::string script_err;

    unified_script_info script = unified_script_loading(get_thread_id(ctx), to_call_fullname, script_err, *get_shim_pointer<shim_map_t>(ctx));

    if(!script.valid)
        return push_error(ctx, script_err);

    #ifdef ENFORCE_PRIVATE
    std::string caller = get_caller(ctx);

    if(!script.in_public && caller != script.owner)
        return push_error(ctx, "Script is private");
    #endif // ENFORCE_PRIVATE

    SL_GUARD(script.seclevel);

    std::string load = script.parsed_source;

    //std::cout << load << std::endl;

    duk_ret_t result = 1;

    stack_duk sd;
    sd.ctx = ctx;

    set_script_info(ctx, to_call_fullname);

    if(!script.is_c_shim)
        compile_and_call(sd, load, get_caller(ctx), false, script.seclevel, false);
    else
    {
        duk_push_c_function(ctx, (*get_shim_pointer<shim_map_t>(ctx))[script.c_shim_name], 1);

        int nargs = 1;

        if(duk_is_undefined(ctx, -2))
        {
            duk_push_object(ctx);
        }
        else
        {
            duk_dup(sd.ctx, -2);
        }

        duk_pcall(ctx, nargs);

        //result = (*get_shim_pointer<shim_map_t>(ctx))[script.c_shim_name](sd.ctx, sl);
    }

    set_script_info(ctx, full_script);

    return result;
}

std::string js_unified_force_call_data(duk_context* ctx, const std::string& data, const std::string& host)
{
    set_script_info(ctx, host + ".invoke");

    stack_duk sd;
    sd.ctx = ctx;

    script_info dummy;
    dummy.load_from_unparsed_source(ctx, attach_wrapper(data, false, true), host + ".invoke", false);

    if(!dummy.valid)
        return "Invalid Command Line Syntax";

    set_global_int(ctx, "last_seclevel", dummy.seclevel);

    duk_push_undefined(ctx);

    std::string extra = compile_and_call(sd, dummy.parsed_source, get_caller(ctx), false, dummy.seclevel, true);

    if(!duk_is_object_coercible(ctx, -1))
    {
        if(extra == "")
            return "No return";
        else
            return extra;
    }

    //std::string ret = duk_json_encode(ctx, -1);

    ///leave the last value on the stack
    duk_dup(ctx, -1);

    std::string ret;

    if(duk_is_string(ctx, -1))
        ret = duk_safe_to_std_string(ctx, -1);
    else
    {
        const char* fnd = duk_json_encode(ctx, -1);

        if(fnd != nullptr)
            ret = std::string(fnd);
        else if(duk_is_undefined(ctx, -1))
            ret = "Command returned Undefined, is there a Syntax Error?";
        else
            ret = "Bad Output, could not be JSON'd";
    }

    ///probably done some gross things to the value on the stack
    ///so we get rid of the duped one
    duk_pop(ctx);

    ret = extra + ret;

    return ret;
}

duk_ret_t err(duk_context* ctx)
{
    push_error(ctx, "Scriptor syntax is the same as function call syntax, do not use .call");
    return 1;
}

std::string add_freeze(const std::string& name)
{
    return " global." + name + " = deepFreeze(global." + name + ");\n";
}

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

void remove_func(duk_context* ctx, const std::string& name)
{
    duk_push_global_object(ctx);

    duk_push_undefined(ctx);
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop(ctx);
}

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
    inject_c_function(ctx, native_print, "print", DUK_VARARGS);

    inject_c_function(ctx, timeout_yield, "timeout_yield",  0);
    inject_c_function(ctx, async_pipe, "async_pipe",  1);
    inject_c_function(ctx, set_is_realtime_script, "set_is_realtime_script", 0);
    inject_c_function(ctx, terminate_realtime, "terminate_realtime", 0);
    inject_c_function(ctx, is_realtime_script, "is_realtime_script", 0);
    inject_c_function(ctx, set_close_window_on_exit, "set_close_window_on_exit", 0);
    inject_c_function(ctx, set_start_window_size, "set_start_window_size", 1);
    inject_c_function(ctx, is_key_down, "is_key_down", 1);
    inject_c_function(ctx, mouse_get_position, "mouse_get_position", 0);

    //fully_freeze(ctx, "hash_d", "db_insert", "db_find", "db_remove", "db_update");
}
