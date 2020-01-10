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
#include "argument_object.hpp"

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
        nlohmann::json data;
        data["type"] = "server_msg";
        data["data"] = str;

        send_async_message(ctx, data.dump());
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
        nlohmann::json data;
        data["type"] = "server_msg";
        data["data"] = str;
        data["no_pad"] = 1;

        send_async_message(ctx, data.dump());
        return push_success(ctx);
    }

	return push_error(ctx, "No pointer or wrong user");
}

void timeout_yield(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();
}

std::string db_insert(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    js::value this_bound = js::get_current_function(*vctx);

    std::string secret_script_host = this_bound.get_hidden("script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(vctx->ctx));
    mongo_ctx.change_collection(secret_script_host);

    std::string json = arg.to_json();

    mongo_ctx->insert_json_one_new(nlohmann::json::parse(json));

    return json;
}

js::value db_update(js::value_context* vctx, js::value json_1_arg, js::value json_2_arg)
{
    js::value current_func = js::get_current_function(*vctx);
    std::string secret_script_host = current_func.get_hidden("script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(vctx->ctx));
    mongo_ctx.change_collection(secret_script_host);

    std::string json_1 = json_1_arg.to_json();
    std::string json_2 = json_2_arg.to_json();

    nlohmann::json j1 = nlohmann::json::parse(json_1);
    nlohmann::json j2 = nlohmann::json::parse(json_2);

    mongo_ctx->update_json_many_new(j1, j2);

    js::value ret(*vctx);
    ret.add("filter", json_1);
    ret.add("update", json_2);
    ret.add("host", secret_script_host);

    return ret;
}

js::value db_find_all(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    js::value current_func = js::get_current_function(*vctx);
    std::string secret_script_host = current_func.get_hidden("script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(vctx->ctx));
    mongo_ctx.change_collection(secret_script_host);

    js::value current_this = js::get_this(*vctx);

    std::string json = current_this.get("JSON");
    std::string proj = current_this.get("PROJ");
    std::string caller = current_this.get("DB_CALLER");

    if(caller != get_caller(vctx->ctx))
        return js::make_error(*vctx, "caller != get_caller() in db_find.array, you probably know what you did");

    std::vector<nlohmann::json> db_data = mongo_ctx->find_json_new(nlohmann::json::parse(json), nlohmann::json::parse(proj));

    return js::make_value(*vctx, db_data);
}

js::value db_find_one(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    js::value current_func = js::get_current_function(*vctx);
    std::string secret_script_host = current_func.get_hidden("script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(vctx->ctx));
    mongo_ctx.change_collection(secret_script_host);

    js::value current_this = js::get_this(*vctx);

    std::string json = current_this.get("JSON");
    std::string proj = current_this.get("PROJ");
    std::string caller = current_this.get("DB_CALLER");

    if(caller != get_caller(vctx->ctx))
        return js::make_error(*vctx, "caller != get_caller() in db_find.array, you probably know what you did");

    std::vector<nlohmann::json> db_data = mongo_ctx->find_json_new(nlohmann::json::parse(json), nlohmann::json::parse(proj));

    if(db_data.size() == 0)
    {
        return js::make_value(*vctx, js::undefined);
    }
    else
    {
        return js::make_value(*vctx, db_data[0]);
    }
}

js::value db_find(js::value_context* vctx, js::value json_obj, js::value proj_obj)
{
    COOPERATE_KILL_VCTX();

    std::string json = "";
    std::string proj = "";

    if(json_obj.is_undefined())
        return js::make_error(*vctx, "First argument must not be undefined");

    json = json_obj.to_json();

    if(!proj_obj.is_undefined())
        proj = std::string("{ \"projection\" : ") + proj_obj.to_json() + " }";
    else
        proj = "{}";

    js::value current_func = js::get_current_function(*vctx);

    std::string script_host = current_func.get_hidden("script_host");

    js::value ret(*vctx);

    ret.add("JSON", json);
    ret.add("PROJ", proj);
    ret.add("DB_CALLER", get_caller(vctx->ctx));

    ret.add("array", js::function<db_find_all>).add_hidden("script_host", script_host);
    ret.add("first", js::function<db_find_one>).add_hidden("script_host", script_host);

    return ret;
}

std::string db_remove(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    js::value current_func = js::get_current_function(*vctx);
    std::string secret_script_host = current_func.get_hidden("script_host");

    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(vctx->ctx));
    mongo_ctx.change_collection(secret_script_host);

    std::string json = arg.to_json();

    mongo_ctx->remove_json_many_new(nlohmann::json::parse(json));

    return json;
}

void set_is_realtime_script(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(vctx->ctx);

    shared_state->set_realtime();

    std::string s1 = get_script_host(vctx->ctx);
    std::string s2 = get_script_ending(vctx->ctx);

    js::value stash = js::get_heap_stash(*vctx);

    js::add_key_value(stash, "realtime_script_name", s1 + "." + s2);
}

void async_pipe(js::value_context* vctx, std::string str)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(vctx->ctx);

    if(str.size() > MAX_MESSAGE_SIZE)
    {
        str.resize(MAX_MESSAGE_SIZE);

        str = str + " [Truncated, > " + std::to_string(MAX_MESSAGE_SIZE) + "]";;
    }

    shared_state->add_output_data(str);
}

bool is_realtime_script(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(vctx->ctx);

    return shared_state->is_realtime();
}

void set_close_window_on_exit(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(vctx->ctx);

    shared_state->set_close_window_on_exit();
}

js::value set_start_window_size(js::value_context* vctx, js::value val)
{
    if(val.is_undefined())
        return js::make_error(*vctx, "Usage: set_start_window_size({width:10, height:25});");

    if(!val.has("width") || !val.has("height"))
        return js::make_error(*vctx, "Must have width *and* height property");

    int width = val.get("width");
    int height = val.get("height");

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(vctx->ctx);

    shared_state->set_width_height(width, height);

    return js::make_success(*vctx);
}

js::value set_realtime_framerate_limit(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    if(!arg.is_number())
        return js::make_error(*vctx, "Usage: set_realtime_framerate_limit(limit)");

    double val = arg;

    if(!isfinite(val))
        return js::make_error(*vctx, "Must be finite");

    val = clamp(val, 1, 60);

    set_global_number(vctx->ctx, "framerate_limit", val);

    return js::make_success(*vctx);
}

void set_is_square_font(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    bool is_square = arg.is_truthy();

    set_global_number(vctx->ctx, "square_font", is_square);
}

bool is_key_down(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    if(!arg.is_string())
        return false;

    std::string str = arg;

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(vctx->ctx);

    return shared_state->is_key_down(str);
}

std::map<std::string, double> mouse_get_position(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = get_shared_worker_state_ptr<shared_duk_worker_state>(vctx->ctx);

    vec2f pos = shared_state->get_mouse_pos();

    pos = clamp(pos, 0.f, 1000.f);

    return {{"x", pos.x()}, {"y", pos.y()}};
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
    set_global_number(ctx, "square_font", 0);

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "DB_ID");

    dukx_put_pointer(ctx, shared_state, "shared_caller_state");

    duk_pop_n(ctx, 1);
}

void teardown_state(duk_context* ctx)
{
    shared_duk_worker_state* shared_state = dukx_get_pointer<shared_duk_worker_state>(ctx, "shared_caller_state");

    delete shared_state;
}

std::string get_string_col(js::value val)
{
    if(!val.is_string())
        return "A";

    std::string str = val;

    return string_to_colour(str);
}

void terminate_realtime(js::value_context* vctx)
{
    shared_duk_worker_state* shared_state = dukx_get_pointer<shared_duk_worker_state>(vctx->ctx, "shared_caller_state");

    shared_state->disable_realtime();
}

void hash_d(js::value_context* vctx, js::value val)
{
    COOPERATE_KILL_VCTX();

    if(get_caller(vctx->ctx) != get_script_host(vctx->ctx))
        return;

    std::string as_json = val.to_json();

    js::value heap_stash = js::get_heap_stash(*vctx);

    js::value hash_d_prop = heap_stash.get("HASH_D");

    std::string fstr = (std::string)hash_d_prop;

    fstr += as_json + "\n";

    hash_d_prop = fstr;
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

///returns true on success, false on failure
bool compile_and_push(duk_context* ctx, const std::string& data)
{
    duk_push_string(ctx, data.c_str());
    duk_push_string(ctx, "test-name");

    return duk_pcompile(ctx, DUK_COMPILE_EVAL) == 0;
}

duk_int_t dukx_pcall_copy(duk_context* ctx, duk_idx_t nargs)
{
    duk_dup(ctx, -1 - nargs);

    for(int i=0; i < nargs; i++)
    {
        duk_dup(ctx, -1 - nargs);
    }

    return duk_pcall(ctx, nargs);
}

std::string compile_and_call(duk_context* ctx, const std::string& data, std::string caller, bool stringify, int seclevel, bool is_top_level, const std::string& calling_script, bool is_cli)
{
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
    //duk_context* new_ctx = (duk_context*)ectx.get_new_context_for(get_script_host(ctx), seclevel);

    duk_idx_t thr_idx = duk_push_thread_new_globalenv(ctx);
    //duk_idx_t thr_idx = duk_push_thread(ctx);
    duk_context* new_ctx = duk_get_context(ctx, thr_idx);
    //register_funcs(new_ctx, seclevel, get_script_host(ctx), true);

    duk_push_object(new_ctx);
    duk_set_global_object(new_ctx);

    std::string wrapper = data;

    //std::cout << "COMPILING " << wrapper << std::endl;

    //exec_stack stk(ectx, new_ctx);

    duk_idx_t fidx = duk_push_thread_new_globalenv(new_ctx);
    duk_context* temporary_ctx = duk_get_context(new_ctx, fidx);
    register_funcs(temporary_ctx, seclevel, get_script_host(ctx), true);

    auto prep_context = [seclevel, caller, script_host, calling_script, base_caller](duk_context* old, duk_context* next)
    {
        duk_push_heap_stash(next);
        duk_push_int(next, seclevel);
        duk_put_prop_string(next, -2, "last_seclevel");
        duk_pop(next);

        duk_idx_t id = duk_push_object(next); ///[object]
        duk_push_string(next, caller.c_str()); ///[object -> caller]
        duk_put_prop_string(next, id, "caller"); ///[object]

        duk_push_string(next, script_host.c_str());
        duk_put_prop_string(next, id, "script_host");

        duk_push_string(next, calling_script.c_str());
        duk_put_prop_string(next, id, "calling_script");

        duk_push_string(next, base_caller.c_str());
        duk_put_prop_string(next, id, "base_caller");

        ///duplicate current object, put it into the global object
        duk_push_global_object(next);
        duk_dup(next, -2);
        duk_put_prop_string(next, -2, "context");
        duk_pop(next);

        //#define USE_PROXY

        ///[object] is on the stack, aka context

        ///this is probably whats breaking the case when ctx == new_ctx
        if(!duk_is_object(old, -2))
            duk_push_undefined(next);
        else
        {
            duk_dup(old, -2);
            ///push args

            #ifndef USE_PROXY
            duk_xmove_top(next, old, 1);
            #else
            dukx_sanitise_move_value(old, next, -1);
            #endif // USE_PROXY
        }

        duk_push_global_object(next);
        duk_dup(next, -2);
        duk_put_prop_string(next, -2, "args");
        duk_pop(next);
    };

    if(!compile_and_push(temporary_ctx, wrapper))
    {
        std::string err = duk_safe_to_string(temporary_ctx, -1);

        duk_pop(temporary_ctx);

        printf("compile failed: %s\n", err.c_str());

        duk_pop(new_ctx);

        //stk.early_out();

        duk_push_string(ctx, "Syntax or Compile Error");
    }
    else
    {
        prep_context(ctx, temporary_ctx);

        duk_push_global_stash(new_ctx);
        duk_get_global_string(temporary_ctx, "Duktape");
        duk_xmove_top(new_ctx, temporary_ctx, 1);
        duk_put_prop_string(new_ctx, -2, DUK_HIDDEN_SYMBOL("module:Duktape"));
        duk_pop(new_ctx);

        int nargs = 2;

        if(!is_cli)
        {
            ///script execution is in two phases
            ///the first phase executes all the requires and returns a function object which is user code
            ///the second phase executes user code
            ///the reason for this is that phase 1 needs to execute in the real global, and phase 2 in the bad global
            duk_int_t ret_requires = dukx_pcall_copy(temporary_ctx, nargs);

            ///don't bother trying to clean up
            if(ret_requires != DUK_EXEC_SUCCESS)
            {
                std::string error_prop = duk_safe_to_std_string(ctx, -1);

                std::cout << "Failed to execute require block " << error_prop << std::endl;

                throw std::runtime_error("Failed to execute require block " + error_prop);
            }

            duk_replace(temporary_ctx, -2 - nargs);
        }
        else
        {
            duk_eval_string(temporary_ctx, "require(\"@babel/polyfill\");");

            duk_push_global_object(temporary_ctx);
            duk_push_c_function(ctx, dummy, 1);
            duk_put_prop_string(temporary_ctx, -2, "require");
            duk_pop(temporary_ctx);
        }

        duk_push_global_object(temporary_ctx);
        duk_get_prop_string(temporary_ctx, -1, "Object");
        duk_del_prop_string(temporary_ctx, -1, "getPrototypeOf");
        duk_pop(temporary_ctx);
        duk_pop(temporary_ctx);

        int moved = 3;

        duk_xmove_top(new_ctx, temporary_ctx, moved);
        duk_remove(new_ctx, -1 - moved); ///removes temporary ctx

        ///now we have [thread] on stack 1, and [object, args] on stack 2
        ///stack 2 has [val] afterwards
        duk_int_t ret_val = duk_pcall(new_ctx, nargs);
        #ifndef USE_PROXY
        duk_xmove_top(ctx, new_ctx, 1);
        #else
        dukx_sanitise_move_value(new_ctx, ctx, -1);
        #endif // USE_PROXY

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

#if 0
js::value js_call(js::value_context* vctx, int sl, js::value arg)
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

    unified_script_info script = unified_script_loading(get_thread_id(ctx), to_call_fullname, script_err);

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
            /*exec_context* ectx = exec_from_ctx(ctx);

            if(ectx == nullptr)
            {
                throw std::runtime_error("Ectx is nullptr in js_call");
            }*/

            compile_and_call(ctx, load, get_caller(ctx), false, script.seclevel, false, full_script, false);
        }

        set_script_info(ctx, full_script);
    }
    else
    {
        duk_push_c_function(ctx, special_scripts::get_special_user_function(script.c_shim_name), 1);

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
#endif // 0

js::value js_call(js::value_context* vctx, int sl, js::value arg)
{
    COOPERATE_KILL_VCTX();

    js::value current_function = js::get_current_function(*vctx);

    std::string secret_script_host = current_function.get_hidden("script_host");

    /*duk_push_current_function(ctx);

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

    duk_pop(ctx);*/

    if(!current_function.has("is_async"))
        return js::make_error(*vctx, "Missing is_async flag");

    if(!current_function.has("FUNCTION_NAME"))
        return js::make_error(*vctx, "Bad script name, this is the developer scolding you, you know what you did");

    std::string to_call_fullname = current_function.get("FUNCTION_NAME");
    bool is_async = current_function.get("is_async");

    if(!is_valid_full_name_string(to_call_fullname))
        return js::make_error(*vctx, "Bad script name, don't do this :)");

    ///current script
    //std::string full_script = get_script_host(ctx) + "." + get_script_ending(ctx);

    std::string full_script = secret_script_host + "." + get_script_ending(vctx->ctx);

    quest_script_data qdata;
    qdata.target = to_call_fullname;

    quest_manager& qm = get_global_quest_manager();

    ///IF IS PRIVILEGED SCRIPT, RETURN THAT CFUNC
    if(privileged_functions.find(to_call_fullname) != privileged_functions.end())
    {
        SL_GUARD(privileged_functions[to_call_fullname].sec_level);

        ///use ORIGINAL script host
        priv_context priv_ctx(get_script_host(vctx->ctx), to_call_fullname);

        //set_script_info(ctx, to_call_fullname);

        js::value arg_dup(*vctx, arg);
        arg_dup.release();

        duk_ret_t result = privileged_functions[to_call_fullname].func(priv_ctx, vctx->ctx, sl);

        js::value ret(*vctx);

        if(result > 0)
        {
            ret = js::value(*vctx, -1);
        }
        else
        {
            ret = js::undefined;
        }

        qm.process(get_thread_id(vctx->ctx), get_caller(vctx->ctx), qdata);

        //set_script_info(ctx, full_script);

        return ret;
    }

    ///so, this indent and everything under if(!script.valid)
    ///needs to be chained into one general "get_script" method

    std::string script_err;

    unified_script_info script = unified_script_loading(get_thread_id(vctx->ctx), to_call_fullname, script_err);

    //std::cout << "script source findy " << script.parsed_source << " name " << script.name << std::endl;

    if(!script.valid)
    {
        std::string err = script_err == "" ? "Tried to run a non existent or invalid script" : script_err;

        return js::make_error(*vctx, err);
    }

    #ifdef ENFORCE_PRIVATE
    std::string caller = get_caller(ctx);

    if(!script.in_public && caller != script.owner)
        return push_error(ctx, "Script is private");
    #endif // ENFORCE_PRIVATE

    SL_GUARD(script.seclevel);

    std::string load = script.parsed_source;

    //std::cout << load << std::endl;

    js::value ret(*vctx);

    if(!script.is_c_shim)
    {
        if(is_async)
        {
            std::shared_ptr<shared_command_handler_state>* shared_state = dukx_get_pointer<std::shared_ptr<shared_command_handler_state>>(vctx->ctx, "all_shared_data");

            if(shared_state == nullptr)
                return js::make_error(*vctx, "Cannot launch async scripts in this context (bot brain, on_breach, or other throwaway script?)");

            set_script_info(vctx->ctx, to_call_fullname);

            std::cout << "launched async\n";

            async_launch_script_name(vctx->ctx, sl, to_call_fullname, *shared_state);

            ret = js::make_success(*vctx);
        }
        else
        {
            set_script_info(vctx->ctx, to_call_fullname);

            js::value arg_dup(*vctx, arg);
            arg_dup.release();

            std::cout << "FULL " << script.name << std::endl;

            compile_and_call(vctx->ctx, load, get_caller(vctx->ctx), false, script.seclevel, false, full_script, false);

            ret = js::value(*vctx, -1);

            std::cout << "ret? " << (std::string)ret << std::endl;
        }

        set_script_info(vctx->ctx, full_script);
    }
    else
    {
        duk_push_c_function(vctx->ctx, special_scripts::get_special_user_function(script.c_shim_name), 1);

        int nargs = 1;

        js::value arg_dup(*vctx, arg);
        arg_dup.release();

        duk_pcall(vctx->ctx, nargs);

        ret = js::value(*vctx, -1);

        //result = (*get_shim_pointer<shim_map_t>(ctx))[script.c_shim_name](sd.ctx, sl);
    }

    qm.process(get_thread_id(vctx->ctx), get_caller(vctx->ctx), qdata);

    return ret;
}

std::string js_unified_force_call_data(exec_context& ectx, const std::string& data, const std::string& host)
{
    duk_context* ctx = (duk_context*)ectx.get_ctx();

    set_script_info(ctx, host + ".invoke");

    std::string unified_invoke_err;

    unified_script_info unified_invoke = unified_script_loading(get_thread_id(ctx), host + ".invoke", unified_invoke_err);

    bool first_invoke_valid = unified_invoke.valid;

    bool is_cli = false;

    if(!unified_invoke.valid || unified_invoke.type == unified_script_info::script_type::BUNDLE)
    {
        script_info dummy;
        dummy.load_from_unparsed_source(ctx, attach_cli_wrapper(data), host + ".invoke", false, true);
        is_cli = true;

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

    std::string extra = compile_and_call(ctx, unified_invoke.parsed_source, get_caller(ctx), false, unified_invoke.seclevel, !first_invoke_valid, "core.invoke", is_cli);

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

/*template<int N>
static
duk_ret_t jxs_call(duk_context* ctx)
{
    int current_seclevel = get_global_int(ctx, "last_seclevel");

    duk_ret_t ret = js_call(ctx, N);

    set_global_int(ctx, "last_seclevel", current_seclevel);

    return ret;
}*/

template<int N>
js::value jxs_call(js::value_context* vctx, js::value val)
{
    int current_seclevel = get_global_int(vctx->ctx, "last_secleve");

    js::value ret = js_call(vctx, N, val);

    set_global_int(vctx->ctx, "last_seclevel", current_seclevel);

    return ret;
}

/*template<int N>
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
}*/

template<int N>
js::value jxos_call(js::value_context* vctx, js::value val)
{
    int current_seclevel = get_global_int(vctx->ctx, "last_seclevel");

    std::vector<std::string> old_caller_stack = get_caller_stack(vctx->ctx);
    std::string old_caller = get_caller(vctx->ctx);

    std::string new_caller = get_script_host(vctx->ctx);

    {
        js::value heap = js::get_heap_stash(*vctx);
        heap.add("caller", new_caller);
        heap.add("caller_stack", std::vector<std::string>{new_caller});
    }

    js::value ret = js_call(vctx, N, val);

    set_global_int(vctx->ctx, "last_seclevel", current_seclevel);

    {
        js::value heap = js::get_heap_stash(*vctx);
        heap.add("caller", old_caller);
        heap.add("caller_stack", old_caller_stack);
    }

    return ret;
}

#if 0
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
#endif // 0

///so ideally this would provide validation
///pass through context and set appropriately
///and modify args
template<int N>
inline
js::value sl_call(js::value_context* vctx, std::string script_name, js::value async)
{
    static_assert(N >= 0 && N <= 4);

    bool async_launch = async.is_truthy();

    js::value val = js::make_value(*vctx, js::function<jxs_call<N>>);
    val.add("FUNCTION_NAME", script_name);
    val.add("call", err);
    val.add("is_async", async_launch);

    js::value current_function = js::get_current_function(*vctx);
    std::string secret_script_host = current_function.get_hidden("script_host");

    val.add_hidden("script_host", secret_script_host);

    return val;
}

/*
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
}*/

template<int N>
inline
js::value os_call(js::value_context* vctx, std::string script_name, js::value async)
{
    static_assert(N >= 0 && N <= 4);

    bool async_launch = async.is_truthy();

    js::value val = js::make_value(*vctx, js::function<jxos_call<N>>);
    val.add("FUNCTION_NAME", script_name);
    val.add("call", err);
    val.add("is_async", async_launch);

    js::value current_function = js::get_current_function(*vctx);
    std::string secret_script_host = current_function.get_hidden("script_host");

    val.add_hidden("script_host", secret_script_host);

    return val;
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

    /*inject_c_function(ctx, os_call<0>, "os_call", 2, "script_host", script_host);

    inject_c_function(ctx, os_call<4>, "ofs_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<3>, "ohs_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<2>, "oms_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<1>, "ols_call", 2, "script_host", script_host);
    inject_c_function(ctx, os_call<0>, "ons_call", 2, "script_host", script_host);*/

    /*if(seclevel <= 4)
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
        inject_c_function(ctx, sl_call<0>, "ns_call", 2, "script_host", script_host);*/

    inject_c_function(ctx, native_print, "print", DUK_VARARGS);
    inject_c_function(ctx, async_print, "async_print", DUK_VARARGS);
    inject_c_function(ctx, async_print_raw, "async_print_raw", DUK_VARARGS);

    js::value_context vctx(ctx);

    js::value global = js::get_global(vctx);
    js::add_key_value(global, "is_realtime_script", js::function<is_realtime_script>);
    js::add_key_value(global, "timeout_yield", js::function<timeout_yield>);
    js::add_key_value(global, "async_pipe", js::function<async_pipe>);
    js::add_key_value(global, "set_is_realtime_script", js::function<set_is_realtime_script>);
    js::add_key_value(global, "set_close_window_on_exit", js::function<set_close_window_on_exit>);
    js::add_key_value(global, "terminate_realtime", js::function<terminate_realtime>);
    js::add_key_value(global, "set_start_window_size", js::function<set_start_window_size>);
    js::add_key_value(global, "set_realtime_framerate_limit", js::function<set_realtime_framerate_limit>);
    js::add_key_value(global, "set_is_square_font", js::function<set_is_square_font>);
    js::add_key_value(global, "is_key_down", js::function<is_key_down>);
    js::add_key_value(global, "mouse_get_position", js::function<mouse_get_position>);
    js::add_key_value(global, "get_string_col", js::function<get_string_col>);
    js::add_key_value(global, "hash_d", js::function<hash_d>);

    if(seclevel <= 4)
        js::add_key_value(global, "db_find", js::function<db_find>).add_hidden("script_host", script_host);

    if(seclevel <= 3)
    {
        js::add_key_value(global, "db_insert", js::function<db_insert>).add_hidden("script_host", script_host);
        js::add_key_value(global, "db_remove", js::function<db_remove>).add_hidden("script_host", script_host);
        js::add_key_value(global, "db_update", js::function<db_update>).add_hidden("script_host", script_host);
    }

    if(seclevel <= 4)
        js::add_key_value(global, "fs_call", js::function<sl_call<4>>).add_hidden("script_host", script_host);

    if(seclevel <= 3)
        js::add_key_value(global, "hs_call", js::function<sl_call<3>>).add_hidden("script_host", script_host);

    if(seclevel <= 2)
        js::add_key_value(global, "ms_call", js::function<sl_call<2>>).add_hidden("script_host", script_host);

    if(seclevel <= 1)
        js::add_key_value(global, "ls_call", js::function<sl_call<1>>).add_hidden("script_host", script_host);

    if(seclevel <= 0)
        js::add_key_value(global, "ns_call", js::function<sl_call<0>>).add_hidden("script_host", script_host);

    js::add_key_value(global, "os_call", js::function<os_call<0>>).add_hidden("script_host", script_host);
    js::add_key_value(global, "ofs_call", js::function<os_call<4>>).add_hidden("script_host", script_host);
    js::add_key_value(global, "ohs_call", js::function<os_call<3>>).add_hidden("script_host", script_host);
    js::add_key_value(global, "oms_call", js::function<os_call<2>>).add_hidden("script_host", script_host);
    js::add_key_value(global, "ols_call", js::function<os_call<1>>).add_hidden("script_host", script_host);
    js::add_key_value(global, "ons_call", js::function<os_call<0>>).add_hidden("script_host", script_host);

    /*#ifdef TESTING
    inject_c_function(ctx, hacky_get, "hacky_get", 0);
    #endif // TESTING*/

    #ifdef TESTING
    inject_c_function(ctx, deliberate_hang, "deliberate_hang", 0);
    #endif // TESTING

    inject_console_log(ctx);

    //#ifdef TESTING
    if(seclevel <= 3)
    {
        dukx_setup_db_proxy(ctx);
    }
    //#endif // TESTING

    if(polyfill)
        dukx_inject_modules(ctx);

    //fully_freeze(ctx, "hash_d", "db_insert", "db_find", "db_remove", "db_update");
}
