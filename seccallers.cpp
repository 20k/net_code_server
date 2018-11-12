#include "seccallers.hpp"
#include "mongo.hpp"
#include "memory_sandbox.hpp"
#include "rate_limiting.hpp"
#include "unified_scripts.hpp"
#include "privileged_core_scripts.hpp"
#include "shared_duk_worker_state.hpp"
#include "duk_object_functions.hpp"
#include "http_beast_server.hpp"
#include "shared_data.hpp"
#include "shared_command_handler_state.hpp"
#include "safe_thread.hpp"
#include "quest_manager.hpp"
#include "duk_modules.hpp"
#include "exec_context.hpp"

int my_timeout_check(void* udata)
{
    COOPERATE_KILL_UDATA(udata);

    return 0;
}

duk_ret_t native_print(duk_context *ctx)
{
	COOPERATE_KILL();

    std::string str = duk_safe_to_std_string(ctx, -1);

    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, "print_str");

    std::string fstr = duk_safe_to_std_string(ctx, -1);

    fstr += str;

    duk_pop_n(ctx, 1);

    duk_push_string(ctx, fstr.c_str());
    duk_put_prop_string(ctx, -2, "print_str");

    duk_pop_n(ctx, 2);

	return 0;
}

duk_ret_t async_print(duk_context* ctx)
{
    COOPERATE_KILL();
    RATELIMIT_DUK(ASYNC_PRINT);

    std::string str;

    int nargs = duk_get_top(ctx);

    for(int i=0; i < nargs; i++)
    {
        if(i != nargs-1)
        {
            str += duk_safe_to_std_string(ctx, i) + " ";
        }
        else
        {
            str += duk_safe_to_std_string(ctx, i);
        }
    }

    command_handler_state* found_ptr = dukx_get_pointer<command_handler_state>(ctx, "command_handler_state_pointer");

    if(found_ptr && get_caller_stack(ctx).size() > 0 && get_caller_stack(ctx)[0] == found_ptr->get_user_name())
    {
        send_async_message(ctx, "command " + str);
        return push_success(ctx);
    }

	return push_error(ctx, "No pointer or wrong user");
}

duk_ret_t async_print_raw(duk_context* ctx)
{
    COOPERATE_KILL();
    RATELIMIT_DUK(ASYNC_PRINT);

    std::string str;

    int nargs = duk_get_top(ctx);

    for(int i=0; i < nargs; i++)
    {
        if(i != nargs-1)
        {
            str += duk_safe_to_std_string(ctx, i) + " ";
        }
        else
        {
            str += duk_safe_to_std_string(ctx, i);
        }
    }

    command_handler_state* found_ptr = dukx_get_pointer<command_handler_state>(ctx, "command_handler_state_pointer");

    if(found_ptr && get_caller_stack(ctx).size() > 0 && get_caller_stack(ctx)[0] == found_ptr->get_user_name())
    {
        send_async_message(ctx, "command_no_pad " + str);
        return push_success(ctx);
    }

	return push_error(ctx, "No pointer or wrong user");
}

duk_ret_t timeout_yield(duk_context* ctx)
{
    COOPERATE_KILL();

    return 0;
}

duk_ret_t db_insert(duk_context* ctx)
{
    COOPERATE_KILL();

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(secret_script_host);

    std::string json = duk_json_encode(ctx, -1);

    mongo_ctx->insert_json_1(secret_script_host, json);

    //std::cout << "json " << json << std::endl;

    return 1;
}

duk_ret_t db_update(duk_context* ctx)
{
    COOPERATE_KILL();

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");

    //std::cout << "SECRET HOST " << secret_script_host << std::endl;

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(secret_script_host);

    std::string json_1 = duk_json_encode(ctx, 0);
    std::string json_2 = duk_json_encode(ctx, 1);

    std::string error = mongo_ctx->update_json_many(secret_script_host, json_1, json_2);

    //std::cout << "update " << json_1 << " with " << json_2 << std::endl;

    push_dukobject(ctx, "filter", json_1, "update", json_2, "error", error, "host", secret_script_host);

    return 1;
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

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(secret_script_host);

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

    std::vector<std::string> db_data = mongo_ctx->find_json(secret_script_host, json, proj);

    parse_push_json(ctx, db_data);

    return 1;
}

duk_ret_t db_find_one(duk_context* ctx)
{
    COOPERATE_KILL();

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(secret_script_host);

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

    std::vector<std::string> db_data = mongo_ctx->find_json(secret_script_host, json, proj);

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
        return push_error(ctx, "Invalid number of args");

    duk_push_object(ctx);

    duk_push_string(ctx, json.c_str());
    duk_put_prop_string(ctx, -2, "JSON");

    duk_push_string(ctx, proj.c_str());
    duk_put_prop_string(ctx, -2, "PROJ");

    duk_push_string(ctx, get_caller(ctx).c_str());
    duk_put_prop_string(ctx, -2, "DB_CALLER");

    //[object]
    /*duk_push_c_function(ctx, db_find_all, 0);
    duk_put_prop_string(ctx, -2, "array");

    duk_push_c_function(ctx, db_find_one, 0);
    duk_put_prop_string(ctx, -2, "first");*/

    dukx_push_c_function_with_hidden(ctx, db_find_all, 0, "script_host", dukx_get_hidden_prop_on_this(ctx, "script_host"));
    duk_put_prop_string(ctx, -2, "array");

    dukx_push_c_function_with_hidden(ctx, db_find_one, 0, "script_host", dukx_get_hidden_prop_on_this(ctx, "script_host"));
    duk_put_prop_string(ctx, -2, "first");

    duk_freeze(ctx, -1);

    return 1;
}


duk_ret_t db_remove(duk_context* ctx)
{
    COOPERATE_KILL();

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));
    mongo_ctx.change_collection(secret_script_host);

    std::string json = duk_json_encode(ctx, -1);

    mongo_ctx->remove_json(secret_script_host, json);

    return 0;
}

duk_ret_t set_is_realtime_script(duk_context* ctx)
{
    COOPERATE_KILL();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    shared_state->set_realtime();

    std::string s1 = get_script_host(ctx);
    std::string s2 = get_script_ending(ctx);

    set_global_string(ctx, "realtime_script_name", s1 + "." + s2);

    return 0;
}

duk_ret_t async_pipe(duk_context* ctx)
{
    COOPERATE_KILL();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(ctx);

    std::string str = duk_safe_to_std_string(ctx, -1);

    if(str.size() > MAX_MESSAGE_SIZE)
    {
        str.resize(MAX_MESSAGE_SIZE);

        str = str + " [Truncated, > " + std::to_string(MAX_MESSAGE_SIZE) + "]";;
    }

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

duk_ret_t set_realtime_framerate_limit(duk_context* ctx)
{
    COOPERATE_KILL();

    if(!duk_is_number(ctx, -1))
        return push_error(ctx, "Usage: set_realtime_framerate_limit(limit)");

    double val = duk_get_number(ctx, -1);

    if(!isfinite(val))
        return push_error(ctx, "Must be finite");

    val = clamp(val, 1, 60);

    set_global_number(ctx, "framerate_limit", val);

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

    push_duk_val(ctx, (bool)shared_state->is_key_down(str));

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
    duk_push_heap_stash(ctx);

    quick_register(ctx, "HASH_D", "");
    quick_register(ctx, "print_str", "");
    quick_register(ctx, "base_caller", caller.c_str());
    quick_register(ctx, "caller", caller.c_str());
    quick_register_generic(ctx, "caller_stack", caller_stack);
    quick_register(ctx, "script_host", script_host.c_str());
    quick_register(ctx, "script_ending", script_ending.c_str());
    set_global_number(ctx, "framerate_limit", 60);

    {
        safe_lock_guard guard(shim_lock);

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

duk_ret_t get_string_col(duk_context* ctx)
{
    if(!duk_is_string(ctx, -1))
        return 0;

    std::string str = duk_safe_to_std_string(ctx, -1);

    std::string c = string_to_colour(str);

    push_duk_val(ctx, c);

    return 1;
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

    duk_push_heap_stash(ctx);
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
    duk_push_heap_stash(ctx);

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
    duk_push_heap_stash(ctx);

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

void send_async_message(duk_context* ctx, const std::string& message)
{
    shared_data* shared = dukx_get_pointer<shared_data>(ctx, "shared_data_ptr");

    if(shared == nullptr)
        return;

    shared->add_back_write(message);
}

duk_ret_t deliberate_hang(duk_context* ctx)
{
    mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

    std::cout << "my id " << *tls_get_thread_id_storage_hack() << std::endl;

    while(1)
    {

    }

    return 0;
}

duk_ret_t global_test(duk_context* ctx)
{
    duk_push_global_object(ctx);
    duk_push_string(ctx, "TEST STRING");
    duk_put_prop_string(ctx, -2, "key");

    duk_push_object(ctx);
    duk_set_global_object(ctx);

    duk_get_prop_string(ctx, -1, "key");
    std::cout << duk_safe_to_std_string(ctx, -1) << std::endl;
    return 1;
}

///returns true on success, false on failure
bool compile_and_push(duk_context* ctx, const std::string& data)
{
    duk_push_string(ctx, data.c_str());
    duk_push_string(ctx, "test-name");

    return duk_pcompile(ctx, DUK_COMPILE_EVAL) == 0;
}

std::string compile_and_call(exec_context& ectx, const std::string& data, std::string caller, bool stringify, int seclevel, bool is_top_level, const std::string& calling_script)
{
    duk_context* ctx = (duk_context*)ectx.ctx;

    if(data.size() == 0)
    {
        duk_push_undefined(ctx);

        return "Script not found";
    }

    std::string script_host = get_script_host(ctx);
    std::string base_caller = get_base_caller(ctx);

    ///bear in mind that under this new system
    ///new_ctx being equal to ctx is very likely
    ///now need to be a bit more careful with object stacks and the like
    duk_context* new_ctx = (duk_context*)ectx.get_new_context_for(get_script_host(ctx), seclevel);

    std::string wrapper = data;

    exec_stack stk(ectx, new_ctx);

    if(!compile_and_push(new_ctx, wrapper))
    {
        std::string err = duk_safe_to_string(new_ctx, -1);

        duk_pop(new_ctx);

        printf("compile failed: %s\n", err.c_str());

        stk.early_out();

        duk_push_string(ctx, "Syntax or Compile Error");
    }
    else
    {
        duk_push_heap_stash(new_ctx);
        duk_push_int(new_ctx, seclevel);
        duk_put_prop_string(new_ctx, -2, "last_seclevel");
        duk_pop(new_ctx);

        duk_idx_t id = duk_push_object(new_ctx); ///[object]
        duk_push_string(new_ctx, caller.c_str()); ///[object -> caller]
        duk_put_prop_string(new_ctx, id, "caller"); ///[object]

        duk_push_string(new_ctx, script_host.c_str());
        duk_put_prop_string(new_ctx, id, "script_host");

        duk_push_string(new_ctx, calling_script.c_str());
        duk_put_prop_string(new_ctx, id, "calling_script");

        duk_push_string(new_ctx, base_caller.c_str());
        duk_put_prop_string(new_ctx, id, "base_caller");

        ///duplicate current object, put it into the global object
        duk_push_global_object(new_ctx);
        duk_dup(new_ctx, -2);
        duk_put_prop_string(new_ctx, -2, "context");
        duk_pop(new_ctx);

        int nargs = 2;

        #define USE_PROXY

        ///[object] is on the stack, aka context

        ///this is probably whats breaking the case when ctx == new_ctx
        if(!duk_is_object(ctx, -2))
            duk_push_undefined(new_ctx);
        else
        {
            duk_dup(ctx, -2);
            ///push args

            #ifndef USE_PROXY
            duk_xmove_top(new_ctx, ctx, 1);
            #else
            dukx_sanitise_move_value(ctx, new_ctx, -1);
            #endif // USE_PROXY
        }

        duk_push_global_object(new_ctx);
        duk_dup(new_ctx, -2);
        duk_put_prop_string(new_ctx, -2, "args");
        duk_pop(new_ctx);

        ///now we have [object, args] on the stack 2

        {
            ///now we have [thread] on stack 1, and [object, args] on stack 2
            ///stack 2 now has [val]
            duk_int_t ret_val = duk_pcall(new_ctx, nargs);

            //if(ret_val == DUK_EXEC_SUCCESS)
            {
                try
                {
                    #ifndef USE_PROXY
                    duk_xmove_top(ctx, new_ctx, 1);
                    #else
                    dukx_sanitise_move_value(new_ctx, ctx, -1);
                    #endif // USE_PROXY
                }
                catch(...)
                {

                }
            }
            /*else
            {
                duk_xmove_top(sd.ctx, new_ctx, 1);
            }*/


            stk.early_out();

            ///stack 2 is now empty, and stack 1 now has [thread, val]
            //duk_xmove_top(sd.ctx, new_ctx, 1);

            bool timeout = is_script_timeout(ctx);

            if(ret_val != DUK_EXEC_SUCCESS && !timeout)
            {
                std::string error_prop;

                if(duk_has_prop_string(ctx, -1, "lineNumber"))
                {
                    error_prop = std::to_string(duk_get_prop_string_as_int(ctx, -1, "lineNumber", 0));
                }

                /*std::string error_stack;

                if(duk_has_prop_string(sd.ctx, -1, "stack"))
                {
                    error_stack = std::to_string(duk_get_prop_string(sd.ctx, -1, "stack"));
                }*/

                std::string err = duk_safe_to_std_string(ctx, -1);

                duk_pop(ctx);

                /*#ifdef TESTING
                error_prop += ". " + error_stack;
                #endif // TESTING*/

                if(error_prop == "")
                    push_dukobject(ctx, "ok", false, "msg", err);
                else
                    push_dukobject(ctx, "ok", false, "msg", err + ". Line Number: " + error_prop);
            }

            if(!is_top_level)
            {
                ///this essentially rethrows an exception
                ///if we're not top level, and we've timedout
                COOPERATE_KILL();
            }

            if(ret_val != DUK_EXEC_SUCCESS && is_top_level && timeout)
            {
                duk_pop(ctx);

                push_dukobject(ctx, "ok", false, "msg", "Ran for longer than 5000ms and timed out");
            }
        }
    }

    ///removes the global
    duk_remove(ctx, -2);

    std::string str = get_hash_d(ctx);

    ///only should do this if the caller is owner of script
    if(str != "" && is_top_level)
    {
        duk_pop(ctx);

        push_duk_val(ctx, str);
    }

    std::string extra = get_print_str(ctx);

    return extra;
}

void async_launch_script_name(duk_context* ctx, int sl, const std::string& sname, std::shared_ptr<shared_command_handler_state>& ptr)
{
    std::string call_end = "s_call(\"" + sname + "\")({});";

    std::string seclevel = "f";

    if(sl == 4)
        seclevel = "f";
    if(sl == 3)
        seclevel = "h";
    if(sl == 2)
        seclevel = "m";
    if(sl == 1)
        seclevel = "l";
    if(sl == 0)
        seclevel = "n";

    //std::cout <<" running " << seclevel + call_end << std::endl;

    sthread sthr(run_in_user_context, get_caller(ctx), seclevel + call_end, ptr, std::nullopt, true);

    sthr.detach();
}

duk_ret_t js_call(duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");
    std::string to_call_fullname;

    duk_push_current_function(ctx);

    bool is_async = false;

    if(!get_duk_keyvalue(ctx, "is_async", is_async))
        return push_error(ctx, "Missing is_async flag");

    //std::cout << "is_async " << is_async << std::endl;

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
    //std::string full_script = get_script_host(ctx) + "." + get_script_ending(ctx);

    std::string full_script = secret_script_host + "." + get_script_ending(ctx);

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

    quest_script_data qdata;
    qdata.target = to_call_fullname;

    quest_manager& qm = get_global_quest_manager();

    ///IF IS PRIVILEGED SCRIPT, RETURN THAT CFUNC
    if(privileged_functions.find(to_call_fullname) != privileged_functions.end())
    {
        SL_GUARD(privileged_functions[to_call_fullname].sec_level);

        ///use ORIGINAL script host
        priv_context priv_ctx(get_script_host(ctx), to_call_fullname);

        //set_script_info(ctx, to_call_fullname);

        duk_ret_t result = privileged_functions[to_call_fullname].func(priv_ctx, ctx, sl);

        qm.process(get_thread_id(ctx), get_caller(ctx), qdata);

        //set_script_info(ctx, full_script);

        return result;
    }

    ///so, this indent and everything under if(!script.valid)
    ///needs to be chained into one general "get_script" method

    std::string script_err;

    unified_script_info script = unified_script_loading(get_thread_id(ctx), to_call_fullname, script_err, *get_shim_pointer<shim_map_t>(ctx));

    //std::cout << "script source findy " << script.parsed_source << " name " << script.name << std::endl;

    if(!script.valid)
    {
        std::string err = script_err == "" ? "Tried to run a non existent or invalid script" : script_err;

        return push_error(ctx, err);
    }

    #ifdef ENFORCE_PRIVATE
    std::string caller = get_caller(ctx);

    if(!script.in_public && caller != script.owner)
        return push_error(ctx, "Script is private");
    #endif // ENFORCE_PRIVATE

    SL_GUARD(script.seclevel);

    std::string load = script.parsed_source;

    //std::cout << load << std::endl;

    duk_ret_t result = 1;

    if(!script.is_c_shim)
    {
        set_script_info(ctx, to_call_fullname);

        if(is_async)
        {
            std::shared_ptr<shared_command_handler_state>* shared_state = dukx_get_pointer<std::shared_ptr<shared_command_handler_state>>(ctx, "all_shared_data");

            if(shared_state == nullptr)
                return push_error(ctx, "Cannot launch async scripts in this context (bot brain, on_breach, or other throwaway script?)");

            std::cout << "launched async\n";

            async_launch_script_name(ctx, sl, to_call_fullname, *shared_state);

            push_success(ctx, "Launched async script");
        }
        else
        {
            exec_context* ectx = exec_from_ctx(ctx);

            if(ectx == nullptr)
            {
                throw std::runtime_error("Ectx is nullptr in js_call");
            }

            compile_and_call(*ectx, load, get_caller(ctx), false, script.seclevel, false, full_script);
        }

        set_script_info(ctx, full_script);
    }
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
            duk_dup(ctx, -2);
        }

        duk_pcall(ctx, nargs);

        //result = (*get_shim_pointer<shim_map_t>(ctx))[script.c_shim_name](sd.ctx, sl);
    }

    qm.process(get_thread_id(ctx), get_caller(ctx), qdata);

    return result;
}

std::string js_unified_force_call_data(exec_context& ectx, const std::string& data, const std::string& host)
{
    duk_context* ctx = (duk_context*)ectx.get_ctx();

    set_script_info(ctx, host + ".invoke");

    std::string unified_invoke_err;

    unified_script_info unified_invoke = unified_script_loading(get_thread_id(ctx), host + ".invoke", unified_invoke_err);

    bool first_invoke_valid = unified_invoke.valid;

    if(!unified_invoke.valid || unified_invoke.type == unified_script_info::script_type::BUNDLE)
    {
        script_info dummy;
        dummy.load_from_unparsed_source(ctx, attach_cli_wrapper(data), host + ".invoke", false, true);

        unified_invoke.make_from(dummy);
    }

    if(!unified_invoke.valid && !first_invoke_valid)
        return unified_invoke_err;

    if(!unified_invoke.valid)
        return "Invalid Command Line Syntax";

    set_global_int(ctx, "last_seclevel", unified_invoke.seclevel);

    if(!first_invoke_valid)
    {
        duk_push_undefined(ctx);
    }
    else
    {
        duk_push_object(ctx);
        push_duk_val(ctx, data);
        duk_put_prop_string(ctx, -2, "command");
    }

    std::string extra = compile_and_call(ectx, unified_invoke.parsed_source, get_caller(ctx), false, unified_invoke.seclevel, !first_invoke_valid, "core.invoke");

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

#if 0
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
#endif // 0

void remove_func(duk_context* ctx, const std::string& name)
{
    duk_push_global_object(ctx);

    duk_push_undefined(ctx);
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop(ctx);
}

duk_ret_t dummy(duk_context* ctx)
{
    return 0;
}

void inject_hacky_Symbol(duk_context* ctx)
{
    duk_push_global_object(ctx);

    duk_push_c_function(ctx, &dummy, 0);
    //duk_put_prop_string(ctx, -2, "Symbol");

    duk_push_string(ctx, "__Symbol_iterator");
    duk_put_prop_string(ctx, -2, "iterator");

    duk_put_prop_string(ctx, -2, "Symbol");

    duk_pop(ctx);
}


template<typename T>
inline
void inject_c_function(duk_context *ctx, T& t, const std::string& str, int nargs)
{
    duk_push_c_function(ctx, &t, nargs);

	duk_put_global_string(ctx, str.c_str());
}

template<typename T, typename... U>
inline
void inject_c_function(duk_context *ctx, T& t, const std::string& str, int nargs, U... u)
{
    duk_push_c_function(ctx, &t, nargs);
	push_dukobject(ctx, u...);
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("HIDDEN_OBJ").c_str());

	duk_put_global_string(ctx, str.c_str());
}

template<int N>
static
duk_ret_t jxs_call(duk_context* ctx)
{
    int current_seclevel = get_global_int(ctx, "last_seclevel");

    duk_ret_t ret = js_call(ctx, N);

    set_global_int(ctx, "last_seclevel", current_seclevel);

    return ret;
}

template<int N>
duk_ret_t jxos_call(duk_context* ctx)
{
    int current_seclevel = get_global_int(ctx, "last_seclevel");

    std::vector<std::string> old_caller_stack = get_caller_stack(ctx);
    std::string old_caller = get_caller(ctx);

    std::string new_caller = get_script_host(ctx);

    duk_push_heap_stash(ctx);

    quick_register(ctx, "caller", new_caller.c_str());
    quick_register_generic(ctx, "caller_stack", std::vector<std::string>{new_caller});

    duk_pop(ctx);

    duk_ret_t ret = js_call(ctx, N);

    set_global_int(ctx, "last_seclevel", current_seclevel);

    duk_push_heap_stash(ctx);

    quick_register(ctx, "caller", old_caller.c_str());
    quick_register_generic(ctx, "caller_stack", old_caller_stack);

    duk_pop(ctx);

    return ret;
}

///so ideally this would provide validation
///pass through context and set appropriately
///and modify args
template<int N>
inline
duk_ret_t sl_call(duk_context* ctx)
{
    static_assert(N >= 0 && N <= 4);

    std::string str = duk_require_string(ctx, -2);

    bool async_launch = dukx_is_truthy(ctx, -1);

    //std::cout << "truthy " << async_launch << std::endl;

    duk_push_c_function(ctx, &jxs_call<N>, 1);

    put_duk_keyvalue(ctx, "FUNCTION_NAME", str);
    put_duk_keyvalue(ctx, "call", err);
    put_duk_keyvalue(ctx, "is_async", async_launch);

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");
    dukx_put_hidden_prop(ctx, -1, "script_host", secret_script_host);

    freeze_duk(ctx);

    return 1;
}

template<int N>
inline
duk_ret_t os_call(duk_context* ctx)
{
    std::string str = duk_require_string(ctx, -2);

    bool async_launch = dukx_is_truthy(ctx, -1);

    ///???
    duk_push_c_function(ctx, &jxos_call<N>, 1);

    put_duk_keyvalue(ctx, "FUNCTION_NAME", str);
    put_duk_keyvalue(ctx, "call", err);
    put_duk_keyvalue(ctx, "is_async", async_launch);

    std::string secret_script_host = dukx_get_hidden_prop_on_this(ctx, "script_host");
    dukx_put_hidden_prop(ctx, -1, "script_host", secret_script_host);

    freeze_duk(ctx);

    return 1;
}

void inject_console_log(duk_context* ctx)
{
    duk_push_global_object(ctx);

    duk_push_object(ctx);
    duk_push_c_function(ctx, async_print, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "log");
    duk_put_prop_string(ctx, -2, "console");

    duk_pop(ctx);
}

void register_funcs(duk_context* ctx, int seclevel, const std::string& script_host, bool polyfill)
{
    remove_func(ctx, "fs_call");
    remove_func(ctx, "hs_call");
    remove_func(ctx, "ms_call");
    remove_func(ctx, "ls_call");
    remove_func(ctx, "ns_call");
    remove_func(ctx, "os_call");

    /*remove_func(ctx, "db_insert");
    remove_func(ctx, "db_find");
    remove_func(ctx, "db_remove");
    remove_func(ctx, "db_update");*/

    inject_c_function(ctx, os_call<0>, "os_call", 2, "script_host", script_host);

    inject_c_function(ctx, os_call<4>, "ofs_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<3>, "ohs_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<2>, "oms_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<1>, "ols_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<0>, "ons_call", 2, "script_host", script_host);

    if(seclevel <= 4)
    {
        inject_c_function(ctx, sl_call<4>, "fs_call", 2, "script_host", script_host);
    }

    if(seclevel <= 3)
        inject_c_function(ctx, sl_call<3>, "hs_call", 2, "script_host", script_host);

    if(seclevel <= 2)
        inject_c_function(ctx, sl_call<2>, "ms_call", 2, "script_host", script_host);

    if(seclevel <= 1)
        inject_c_function(ctx, sl_call<1>, "ls_call", 2, "script_host", script_host);

    if(seclevel <= 0)
        inject_c_function(ctx, sl_call<0>, "ns_call", 2, "script_host", script_host);

    inject_c_function(ctx, hash_d, "hash_d", 1);

    if(seclevel <= 3)
    {
        inject_c_function(ctx, db_insert, "db_insert", 1, "script_host", script_host);
        inject_c_function(ctx, db_find, "db_find", DUK_VARARGS, "script_host", script_host);
        inject_c_function(ctx, db_remove, "db_remove", 1, "script_host", script_host);
        inject_c_function(ctx, db_update, "db_update", 2, "script_host", script_host);
    }

    inject_c_function(ctx, native_print, "print", DUK_VARARGS);
    inject_c_function(ctx, async_print, "async_print", DUK_VARARGS);
    inject_c_function(ctx, async_print_raw, "async_print_raw", DUK_VARARGS);

    inject_c_function(ctx, timeout_yield, "timeout_yield",  0);
    inject_c_function(ctx, async_pipe, "async_pipe",  1);
    inject_c_function(ctx, set_is_realtime_script, "set_is_realtime_script", 0);
    inject_c_function(ctx, terminate_realtime, "terminate_realtime", 0);
    inject_c_function(ctx, is_realtime_script, "is_realtime_script", 0);
    inject_c_function(ctx, set_close_window_on_exit, "set_close_window_on_exit", 0);
    inject_c_function(ctx, set_start_window_size, "set_start_window_size", 1);
    inject_c_function(ctx, is_key_down, "is_key_down", 1);
    inject_c_function(ctx, mouse_get_position, "mouse_get_position", 0);
    inject_c_function(ctx, get_string_col, "get_string_col", 1);
    inject_c_function(ctx, set_realtime_framerate_limit, "set_realtime_framerate_limit", 1);

    /*#ifdef TESTING
    inject_c_function(ctx, hacky_get, "hacky_get", 0);
    #endif // TESTING*/

    #ifdef TESTING
    inject_c_function(ctx, deliberate_hang, "deliberate_hang", 0);
    inject_c_function(ctx, global_test, "global_test", 0);
    #endif // TESTING

    inject_console_log(ctx);

    //#ifdef TESTING
    if(seclevel <= 3)
    {
        dukx_setup_db_proxy(ctx);
    }
    //#endif // TESTING

    inject_hacky_Symbol(ctx);

    if(polyfill)
        dukx_inject_modules(ctx);

    //fully_freeze(ctx, "hash_d", "db_insert", "db_find", "db_remove", "db_update");
}
