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
#include "argument_object.hpp"
#include "command_handler_fiber_backend.hpp"
#include "time.hpp"
#include "timestamped_event_queue.hpp"
#include "js_ui.hpp"

///still needs to be defined while we're compiling duktape
int my_timeout_check(void* udata)
{
    if(udata == nullptr)
        return 0;

    COOPERATE_KILL_UDATA(udata);

    return 0;
}

#ifdef USE_QUICKJS
int interrupt_handler(JSRuntime* rt, void* udata)
{
    if(udata == nullptr)
        return 0;

    sandbox_data* sand_data = (sandbox_data*)udata;
    if(sand_data->terminate_semi_gracefully)
    { printf("Cooperating with kill udata\n");
        return 1;
    }
    if(sand_data->terminate_realtime_gracefully)
    { printf("Cooperating with kill realtime\n");
        return 1;
    }
    try
    {
        handle_sleep(sand_data);

        /*if(JS_IsJobPending(rt))
        {
            JSContext* pending = nullptr;

            int res = JS_ExecutePendingJob(rt, &pending);

            if(res < 0)
                return 1;
        }*/
    }
    catch(...)
    {
        return 1;
    }

    return 0;
}
#endif // USE_QUICKJS

js::value get_current_user(js::value_context* vctx)
{
    js::value val(*vctx);

    val = get_caller(*vctx);

    return val;
}

void dummy(js::value_context* vctx)
{

}

void native_print(js::value_context* vctx, std::string str)
{
    COOPERATE_KILL_VCTX();

    js::value heap = js::get_heap_stash(*vctx);

    js::value print_obj = heap.get("print_str");

    std::string print_str = print_obj;

    print_str += str;

    print_obj = print_str;
}

js::value async_print(js::value_context* vctx, std::string what)
{
    COOPERATE_KILL_VCTX();
    RATELIMIT_VPDUK(ASYNC_PRINT);

    command_handler_state* found_ptr = js::get_heap_stash(*vctx)["command_handler_state_pointer"].get_ptr<command_handler_state>();

    if(found_ptr && get_caller_stack(*vctx).size() > 0 && get_caller_stack(*vctx)[0] == found_ptr->get_user_name())
    {
        nlohmann::json data;
        data["type"] = "server_msg";
        data["data"] = what;

        send_async_message(*vctx, data.dump());
        return js::make_success(*vctx);
    }

	return js::make_error(*vctx, "No pointer or wrong user");
}

void make_console_log(js::value_context& vctx)
{
    std::string program =
R"(
   console = {
      log : function(...xs)
       {
           var args = xs.join(" ");

           async_print(args);
       }
   };
)";

    js::eval(vctx, program);
}

js::value async_print_raw(js::value_context* vctx, std::string what)
{
    COOPERATE_KILL_VCTX();
    RATELIMIT_VPDUK(ASYNC_PRINT);

    command_handler_state* found_ptr = js::get_heap_stash(*vctx)["command_handler_state_pointer"].get_ptr<command_handler_state>();

    if(found_ptr && get_caller_stack(*vctx).size() > 0 && get_caller_stack(*vctx)[0] == found_ptr->get_user_name())
    {
        nlohmann::json data;
        data["type"] = "server_msg";
        data["data"] = what;
        data["no_pad"] = 1;

        send_async_message(*vctx, data.dump());
        return js::make_success(*vctx);
    }

	return js::make_error(*vctx, "No pointer or wrong user");
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

    disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
    disk_ctx.change_collection(secret_script_host);

    std::string json = arg.to_json();

    disk_ctx->insert_json_one_new(nlohmann::json::parse(json));

    return json;
}

js::value db_update(js::value_context* vctx, js::value json_1_arg, js::value json_2_arg)
{
    js::value current_func = js::get_current_function(*vctx);
    std::string secret_script_host = current_func.get_hidden("script_host");

    disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
    disk_ctx.change_collection(secret_script_host);

    std::string json_1 = json_1_arg.to_json();
    std::string json_2 = json_2_arg.to_json();

    nlohmann::json j1 = nlohmann::json::parse(json_1);
    nlohmann::json j2 = nlohmann::json::parse(json_2);

    disk_ctx->update_json_many_new(j1, j2);

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

    disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
    disk_ctx.change_collection(secret_script_host);

    js::value current_this = js::get_this(*vctx);

    std::string json = current_this.get("JSON");
    std::string proj = current_this.get("PROJ");
    std::string caller = current_this.get("DB_CALLER");

    if(caller != get_caller(*vctx))
        return js::make_error(*vctx, "caller != get_caller() in db_find.array, you probably know what you did");

    std::vector<nlohmann::json> db_data = disk_ctx->find_json_new(nlohmann::json::parse(json), nlohmann::json::parse(proj));

    return js::make_value(*vctx, db_data);
}

js::value db_find_one(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    js::value current_func = js::get_current_function(*vctx);
    std::string secret_script_host = current_func.get_hidden("script_host");

    disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
    disk_ctx.change_collection(secret_script_host);

    js::value current_this = js::get_this(*vctx);

    std::string json = current_this.get("JSON");
    std::string proj = current_this.get("PROJ");
    std::string caller = current_this.get("DB_CALLER");

    if(caller != get_caller(*vctx))
        return js::make_error(*vctx, "caller != get_caller() in db_find.array, you probably know what you did");

    std::vector<nlohmann::json> db_data = disk_ctx->find_json_new(nlohmann::json::parse(json), nlohmann::json::parse(proj));

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
    ret.add("DB_CALLER", get_caller(*vctx));

    ret.add("array", js::function<db_find_all>).add_hidden("script_host", script_host);
    ret.add("first", js::function<db_find_one>).add_hidden("script_host", script_host);

    return ret;
}

std::string db_remove(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    js::value current_func = js::get_current_function(*vctx);
    std::string secret_script_host = current_func.get_hidden("script_host");

    disk_nolock_proxy disk_ctx = get_global_disk_user_accessible_context();
    disk_ctx.change_collection(secret_script_host);

    std::string json = arg.to_json();

    disk_ctx->remove_json_many_new(nlohmann::json::parse(json));

    return json;
}

void set_is_realtime_script(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

    shared_state->set_realtime();

    std::string s1 = get_script_host(*vctx);
    std::string s2 = get_script_ending(*vctx);

    js::value stash = js::get_heap_stash(*vctx);

    js::add_key_value(stash, "realtime_script_name", s1 + "." + s2);
}

void async_pipe(js::value_context* vctx, std::string str)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

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

    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

    return shared_state->is_realtime();
}

void set_close_window_on_exit(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

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

    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

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

    js::get_heap_stash(*vctx).get("framerate_limit") = val;

    return js::make_success(*vctx);
}

void set_is_square_font(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    bool is_square = arg.is_truthy();

    js::get_heap_stash(*vctx).get("square_font") = (int)is_square;
}

bool is_key_down(js::value_context* vctx, js::value arg)
{
    COOPERATE_KILL_VCTX();

    if(!arg.is_string())
        return false;

    std::string str = arg;

    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

    return shared_state->is_key_down(str);
}

std::map<std::string, double> mouse_get_position(js::value_context* vctx)
{
    COOPERATE_KILL_VCTX();

    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

    vec2f pos = shared_state->get_mouse_pos();

    pos = clamp(pos, 0.f, 1000.f);

    return {{"x", pos.x()}, {"y", pos.y()}};
}

void startup_state(js::value_context& vctx, const std::string& caller, const std::string& script_host, const std::string& script_ending, const std::vector<std::string>& caller_stack, shared_duk_worker_state* shared_state)
{
    js::value heap = js::get_heap_stash(vctx);
    heap["HASH_D"] = "";
    heap["print_str"] = "";
    heap["base_caller"] = caller;
    heap["caller"] = caller;
    heap["caller_stack"] = caller_stack;
    heap["script_host"] = script_host;
    heap["script_ending"] = script_ending;
    heap["framerate_limit"] = 60;
    heap["square_font"] = 0;
    heap["DB_ID"] = 0;
    heap["shared_caller_state"].set_ptr(shared_state);

    js_ui::ui_stack* stk = new js_ui::ui_stack;
    heap["ui_stack"].set_ptr(stk);
    heap["blank_ui_is_significant"] = 0;
}

void teardown_state(js::value_context& vctx)
{
    shared_duk_worker_state* shared_state = js::get_heap_stash(vctx).get("shared_caller_state").get_ptr<shared_duk_worker_state>();
    delete shared_state;

    js_ui::ui_stack* stk = js::get_heap_stash(vctx).get("ui_stack").get_ptr<js_ui::ui_stack>();
    delete stk;
}

std::string get_string_col(js::value_context* ctx, js::value val)
{
    if(!val.is_string())
        return "A";

    std::string str = val;

    return string_to_colour(str);
}

void terminate_realtime(js::value_context* vctx)
{
    shared_duk_worker_state* shared_state = js::get_heap_stash(*vctx)["shared_caller_state"].get_ptr<shared_duk_worker_state>();

    shared_state->disable_realtime();
}

void hash_d(js::value_context* vctx, js::value val)
{
    COOPERATE_KILL_VCTX();

    if(get_caller(*vctx) != get_script_host(*vctx))
        return;

    std::string as_json = val.to_json();

    js::value heap_stash = js::get_heap_stash(*vctx);

    js::value hash_d_prop = heap_stash.get("HASH_D");

    std::string fstr = (std::string)hash_d_prop;

    fstr += as_json + "\n";

    hash_d_prop = fstr;
}

std::string get_hash_d(js::value_context* vctx)
{
    js::value heap = js::get_heap_stash(*vctx);

    if(!heap.has("HASH_D"))
        return "";

    return (std::string)heap.get("HASH_D");
}

std::string get_print_str(js::value_context* vctx)
{
    js::value heap = js::get_heap_stash(*vctx);

    if(!heap.has("print_str"))
        return "";

    return (std::string)heap.get("print_str");
}

void send_async_message(js::value_context& vctx, const std::string& message)
{
    js::value heap = js::get_heap_stash(vctx);

    shared_data* shared = heap["shared_data_ptr"].get_ptr<shared_data>();

    if(shared == nullptr)
        return;

    shared->add_back_write(message);
}

std::pair<std::string, js::value> compile_and_call(js::value_context& vctx, js::value& arg, const std::string& data, std::string caller, int seclevel, bool is_top_level, const std::string& calling_script, const std::string& script_name, bool is_cli)
{
    if(data.size() == 0)
    {
        return {"Script not found", js::make_value(vctx, js::undefined)};
    }

    js::value ret(vctx);

    std::string script_host = get_script_host(vctx);
    std::string base_caller = get_base_caller(vctx);

    js::value_context new_vctx(vctx);

    {
        js::value new_global(new_vctx);
        js::set_global(new_vctx, new_global);
    }

    std::string wrapper = data;


    js::value_context temporary_vctx(new_vctx);

    //#ifdef USE_DUKTAPE
    register_funcs(temporary_vctx, seclevel, get_script_host(vctx), true);
    //#endif // USE_DUKTAPE

    /*#ifdef USE_QUICKJS
    js::value next_heap(temporary_vctx);
    next_heap.add("last_seclevel", seclevel);

    js::value context(temporary_vctx);

    context.add("caller", caller);
    context.add("script_host", script_host);
    context.add("calling_script", calling_script);
    context.add("base_caller", base_caller);

    js::value glob = js::get_global(temporary_vctx);
    js::add_key_value(glob, "context", context);

    js::value next_arg(temporary_vctx);

    if(!arg.is_object_coercible())
        next_arg = js::undefined;
    else
        next_arg = js::xfer_between_contexts(temporary_vctx, arg);

    js::add_key_value(glob, "args", next_arg);

    js::value temp_ret(temporary_vctx);
    temp_ret = js::eval(temporary_vctx, wrapper);

    bool err = temp_ret.is_error() || temp_ret.is_exception();

    if(err)
    {
        temp_ret = temp_ret.to_error_message();
    }

    std::cout << temp_ret.to_json() << std::endl;

    ret = js::xfer_between_contexts(vctx, temp_ret);

    #else*/
    auto [compile_success, compiled_func] = js::compile(temporary_vctx, wrapper, script_name);

    if(!compile_success)
    {
        std::string err = compiled_func.to_error_message();

        printf("compile failed: %s\n", err.c_str());

        ret = js::make_value(vctx, "Syntax or Compile Error: " + err);
    }
    else
    {
        js::value next_heap(temporary_vctx);
        next_heap.add("last_seclevel", seclevel);

        js::value context(temporary_vctx);

        context.add("caller", caller);
        context.add("script_host", script_host);
        context.add("calling_script", calling_script);
        context.add("base_caller", base_caller);

        js::value glob = js::get_global(temporary_vctx);
        js::add_key_value(glob, "context", context);

        js::value next_arg(temporary_vctx);

        if(!arg.is_object_coercible())
            next_arg = js::undefined;
        else
            next_arg = js::xfer_between_contexts(temporary_vctx, arg);

        js::add_key_value(glob, "args", next_arg);

        ///THING I DONT REALLY UNDERSTAND BUT IS NECESSARY FOR STUFF TO WORK
        #ifdef USE_DUKTAPE
        {
            js::value temp_global = js::get_global(temporary_vctx);
            js::value duktape_sym = temp_global.get("Duktape");

            js::value xferred = js::xfer_between_contexts(new_vctx, duktape_sym);

            js::value new_global_stash = js::get_global_stash(new_vctx);

            js::add_key_value(new_global_stash, "\xFFmodule:Duktape", xferred);
        }
        #endif // USE_DUKTAPE

        js::value temp_ret(temporary_vctx);

        if(!is_cli)
        {
            auto [success, retval] = js::call_compiled(compiled_func);

            if(!success)
            {
                std::cout << "Failed to execute require block " << (std::string)retval << std::endl;
                throw std::runtime_error("Failed to execute require block " + (std::string)retval);
            }

            temp_ret = retval;
        }
        else
        {
            #ifdef USE_DUKTAPE
            js::eval(temporary_vctx, "require(\"@babel/polyfill\");");

            {
                js::value glob = js::get_global(temporary_vctx);
                js::add_key_value(glob, "require", js::function<dummy>);
            }
            #endif // USE_DUKTAPE

            temp_ret = compiled_func;
        }

        {
            js::value temp_global = js::get_global(temporary_vctx);
            js::value js_object = temp_global.get("Object");
            js_object.del("getPrototypeOf");
        }

        js::value new_func = js::xfer_between_contexts(new_vctx, temp_ret);
        js::value new_context = js::xfer_between_contexts(new_vctx, context);
        js::value new_args = js::xfer_between_contexts(new_vctx, next_arg);

        {
            js::value glob = js::get_global(new_vctx);
            js::add_key_value(glob, "context", new_context);
            js::add_key_value(glob, "args", new_args);
        }

        bool success = false;
        js::value found_val(new_vctx);

        if(is_cli)
        {
            std::tie(success, found_val) = js::call_compiled(new_func);
        }
        else
        {
            std::tie(success, found_val) = js::call(new_func);
        }

        //auto [success, found_val] = js::call(new_func, new_context, new_args);

        /*#ifndef USE_PROXY
        duk_xmove_top(ctx, new_ctx, 1);
        #else
        dukx_sanitise_move_value(new_ctx, ctx, -1);
        #endif // USE_PROXY*/

        ret = js::xfer_between_contexts(vctx, found_val);

        bool timeout = is_script_timeout(vctx);

        if(!success && !timeout)
        {
            ret = js::make_error(vctx, ret.to_error_message());
        }

        if(!is_top_level)
        {
            ///this essentially rethrows an exception
            ///if we're not top level, and we've timedout
            //COOPERATE_KILL_UDATA(js::get_sandbox_data<sandbox_data>(vctx));
        }

        if(!success && is_top_level && timeout)
        {
            ret = js::make_error(vctx, "Ran for longer than 5000ms and timed out");
        }
    }
    //#endif

    std::string str = get_hash_d(&vctx);

    ///only should do this if the caller is owner of script
    if(str != "" && is_top_level)
    {
        ret = js::make_value(vctx, str);
    }

    std::string extra = get_print_str(&vctx);

    return {extra, ret};
}

void async_launch_script_name(js::value_context& vctx, int sl, const std::string& sname, const std::string& args_json, std::shared_ptr<shared_command_handler_state>& ptr)
{
    std::string call_end = "s_call(\"" + sname + "\")(" + args_json + ");";

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

    std::string caller = get_caller(vctx);

    #ifndef USE_FIBERS

    sthread sthr(run_in_user_context, get_caller(vctx), seclevel + call_end, ptr, std::nullopt, true);
    sthr.detach();

    #else

    get_global_fiber_queue().add([=]()
    {
        run_in_user_context(caller, seclevel + call_end, ptr, std::nullopt, true);
    });

    #endif // USE_FIBERS
}

js::value js_call(js::value_context* vctx, int sl, js::value arg)
{
    COOPERATE_KILL_VCTX();

    js::value current_function = js::get_current_function(*vctx);

    std::string secret_script_host = current_function.get_hidden("script_host");

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

    std::string full_script = secret_script_host + "." + get_script_ending(*vctx);

    quest_script_data qdata;
    qdata.target = to_call_fullname;

    quest_manager& qm = get_global_quest_manager();

    ///IF IS PRIVILEGED SCRIPT, RETURN THAT CFUNC
    if(privileged_functions.find(to_call_fullname) != privileged_functions.end())
    {
        auto it = privileged_functions.find(to_call_fullname);

        SL_GUARD(it->second.sec_level);

        ///use ORIGINAL script host
        priv_context priv_ctx(get_script_host(*vctx), to_call_fullname);

        //set_script_info(ctx, to_call_fullname);

        /*if(it->second.func_duk)
        {
            js::value arg_dup(*vctx, arg);
            arg_dup.release();

            duk_ret_t result = it->second.func_duk(priv_ctx, vctx->ctx, sl);

            js::value ret(*vctx);

            if(result > 0)
            {
                ret = js::value(*vctx, -1);
            }
            else
            {
                ret = js::undefined;
            }

            qm.process(get_thread_id(*vctx), get_caller(*vctx), qdata);

            return ret;
        }
        else*/
        {
            assert(it->second.func_new);

            js::value ret = it->second.func_new(priv_ctx, *vctx, arg, sl);

            qm.process(get_thread_id(*vctx), get_caller(*vctx), qdata);

            return ret;
        }

        //set_script_info(ctx, full_script);
    }

    ///so, this indent and everything under if(!script.valid)
    ///needs to be chained into one general "get_script" method

    std::string script_err;

    unified_script_info script = unified_script_loading(get_thread_id(*vctx), to_call_fullname, script_err);

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
            js::value heap_stash = js::get_heap_stash(*vctx);

            std::shared_ptr<shared_command_handler_state>* shared_state = heap_stash["all_shared_data"].get_ptr<std::shared_ptr<shared_command_handler_state>>();

            if(shared_state == nullptr)
                return js::make_error(*vctx, "Cannot launch async scripts in this context (bot brain, on_breach, or other throwaway script?)");

            set_script_info(*vctx, to_call_fullname);

            std::cout << "launched async\n";

            async_launch_script_name(*vctx, sl, to_call_fullname, arg.to_json(), *shared_state);

            ret = js::make_success(*vctx);
        }
        else
        {
            set_script_info(*vctx, to_call_fullname);

            auto [msg, res] = compile_and_call(*vctx, arg, load, get_caller(*vctx), script.seclevel, false, full_script, script.name, false);

            res.pack();

            ret = std::move(res);
        }

        set_script_info(*vctx, full_script);
    }
    else
    {
        ///might as well be void*
        js::funcptr_t erased_funcptr = special_scripts::get_special_user_function(script.c_shim_name);

        js::value func = js::make_value(*vctx, erased_funcptr);

        auto [success, rval] = js::call(func, arg);

        ret = std::move(rval);

        //result = (*get_shim_pointer<shim_map_t>(ctx))[script.c_shim_name](sd.ctx, sl);
    }

    qm.process(get_thread_id(*vctx), get_caller(*vctx), qdata);

    return ret;
}

std::pair<js::value, std::string> js_unified_force_call_data(js::value_context& vctx, const std::string& data, const std::string& host)
{
    set_script_info(vctx, host + ".invoke");

    std::string unified_invoke_err;

    unified_script_info unified_invoke = unified_script_loading(get_thread_id(vctx), host + ".invoke", unified_invoke_err);

    bool first_invoke_valid = unified_invoke.valid;

    bool is_cli = false;

    if(!unified_invoke.valid || unified_invoke.type == unified_script_info::script_type::BUNDLE)
    {
        script_info dummy;
        dummy.load_from_unparsed_source(vctx, attach_cli_wrapper(data), host + ".invoke", false, true);
        is_cli = true;

        unified_invoke.make_from(dummy);
    }

    if(!unified_invoke.valid && !first_invoke_valid)
        return {js::make_value(vctx, js::undefined), unified_invoke_err};

    if(!unified_invoke.valid)
        return {js::make_value(vctx, js::undefined), "Invalid Command Line Syntax"};

    js::get_heap_stash(vctx).add("last_seclevel", unified_invoke.seclevel);

    js::value arg(vctx);

    if(!first_invoke_valid)
        arg = js::undefined;
    else
        js::add_key_value(arg, "command", data);

    auto [extra, js_val] = compile_and_call(vctx, arg, unified_invoke.parsed_source, get_caller(vctx), unified_invoke.seclevel, !first_invoke_valid, "core.invoke", unified_invoke.name, is_cli);

    #ifdef USE_QUICKJS
    js_val = js::execute_promises(vctx, js_val);
    #endif // USE_QUICKJS

    if(!js_val.is_object_coercible())
    {
        if(extra == "")
            return {js_val, "No return"};
        else
            return {js_val, extra};
    }

    std::string ret;

    if(js_val.is_string())
    {
        ret = (std::string)js_val;
    }
    else
    {
        if(js_val.is_undefined())
            ret = "Command returned \"undefined\", syntax error?";

        std::string as_json = js_val.to_json();

        if(as_json == "")
            ret = "Bad Output, could not be JSON'd";
        else
            ret = as_json;
    }

    ret = extra + ret;

    return {js_val, ret};
}

js::value err(js::value_context* vctx)
{
    return js::make_error(*vctx, "Scriptor syntax is the same as function call syntax, do not use .call");
}

template<int N>
js::value jxs_call(js::value_context* vctx, js::value val)
{
    int current_seclevel = js::get_heap_stash(*vctx).get("last_seclevel");

    js::value ret = js_call(vctx, N, val);

    js::get_heap_stash(*vctx).get("last_seclevel") = current_seclevel;

    return ret;
}

template<int N>
js::value jxos_call(js::value_context* vctx, js::value val)
{
    int current_seclevel = js::get_heap_stash(*vctx).get("last_seclevel");

    std::vector<std::string> old_caller_stack = get_caller_stack(*vctx);
    std::string old_caller = get_caller(*vctx);

    js::value current_function = js::get_current_function(*vctx);

    std::string new_caller = current_function.get_hidden("script_host");

    {
        js::value heap = js::get_heap_stash(*vctx);
        heap.add("caller", new_caller);
        heap.add("caller_stack", std::vector<std::string>{new_caller});
    }

    js::value ret = js_call(vctx, N, val);

    js::get_heap_stash(*vctx).get("last_seclevel") = current_seclevel;

    {
        js::value heap = js::get_heap_stash(*vctx);
        heap.add("caller", old_caller);
        heap.add("caller_stack", old_caller_stack);
    }

    return ret;
}

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
    val.add("call", js::function<err>);
    val.add("is_async", async_launch);

    js::value current_function = js::get_current_function(*vctx);
    std::string secret_script_host = current_function.get_hidden("script_host");

    val.add_hidden("script_host", secret_script_host);

    return val;
}

template<int N>
inline
js::value os_call(js::value_context* vctx, std::string script_name, js::value async)
{
    static_assert(N >= 0 && N <= 4);

    bool async_launch = async.is_truthy();

    js::value val = js::make_value(*vctx, js::function<jxos_call<N>>);
    val.add("FUNCTION_NAME", script_name);
    val.add("call", js::function<err>);
    val.add("is_async", async_launch);

    js::value current_function = js::get_current_function(*vctx);
    std::string secret_script_host = current_function.get_hidden("script_host");

    val.add_hidden("script_host", secret_script_host);

    return val;
}

void register_funcs(js::value_context& vctx, int seclevel, const std::string& script_host, bool polyfill)
{
    js::value global = js::get_global(vctx);

    global.del("fs_call");
    global.del("hs_call");
    global.del("ms_call");
    global.del("ls_call");
    global.del("ns_call");
    global.del("os_call");
    global.del("ofs_call");
    global.del("ohs_call");
    global.del("oms_call");
    global.del("ols_call");
    global.del("ons_call");

    {
        #ifdef USE_QUICKJS
        std::vector<std::pair<js::value, js::value>> val = global.iterate();

        for(auto& i : val)
        {
            if(!i.second.has("prototype"))
                continue;

            if(!i.second["prototype"].has("constructor"))
                continue;

            i.second["prototype"].del("constructor");
        }
        #endif // USE_QUICKJS
    }

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

    assert(global.has("hash_d"));

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

    js::add_key_value(global, "print", js::function<native_print>);
    js::add_key_value(global, "async_print", js::function<async_print>);
    js::add_key_value(global, "async_print_raw", js::function<async_print_raw>);
    js::add_key_value(global, "get_current_user", js::function<get_current_user>);

    js::value imgui_obj(vctx);
    imgui_obj["text"] = js::function<js_ui::text>;
    imgui_obj["textcolored"] = js::function<js_ui::textcolored>;
    imgui_obj["textdisabled"] = js::function<js_ui::textdisabled>;
    imgui_obj["bullettext"] = js::function<js_ui::bullettext>;
    imgui_obj["smallbutton"] = js::function<js_ui::smallbutton>;
    imgui_obj["invisiblebutton"] = js::function<js_ui::invisiblebutton>;
    imgui_obj["arrowbutton"] = js::function<js_ui::arrowbutton>;
    imgui_obj["button"] = js::function<js_ui::button>;
    imgui_obj["checkbox"] = js::function<js_ui::checkbox>;
    imgui_obj["radiobutton"] = js::function<js_ui::radiobutton>;
    imgui_obj["progressbar"] = js::function<js_ui::progressbar>;
    imgui_obj["bullet"] = js::function<js_ui::bullet>;
    imgui_obj["pushstylecolor"] = js::function<js_ui::pushstylecolor>;
    imgui_obj["popstylecolor"] = js::function<js_ui::popstylecolor>;

    imgui_obj["pushitemwidth"] = js::function<js_ui::pushitemwidth>;
    imgui_obj["popitemwidth"] = js::function<js_ui::popitemwidth>;
    imgui_obj["setnextitemwidth"] = js::function<js_ui::setnextitemwidth>;

    imgui_obj["separator"] = js::function<js_ui::separator>;
    imgui_obj["sameline"] = js::function<js_ui::sameline>;
    imgui_obj["newline"] = js::function<js_ui::newline>;
    imgui_obj["spacing"] = js::function<js_ui::spacing>;
    imgui_obj["dummy"] = js::function<js_ui::dummy>;
    imgui_obj["indent"] = js::function<js_ui::indent>;
    imgui_obj["unindent"] = js::function<js_ui::unindent>;
    imgui_obj["begingroup"] = js::function<js_ui::begingroup>;
    imgui_obj["endgroup"] = js::function<js_ui::endgroup>;
    imgui_obj["isitemclicked"] = js::function<js_ui::isitemclicked>;
    imgui_obj["isitemhovered"] = js::function<js_ui::isitemhovered>;

    imgui_obj["dragfloat"] = js::function<js_ui::dragfloat>;
    imgui_obj["dragfloat2"] = js::function<js_ui::dragfloat2>;
    imgui_obj["dragfloat3"] = js::function<js_ui::dragfloat3>;
    imgui_obj["dragfloat4"] = js::function<js_ui::dragfloat4>;
    imgui_obj["dragint"] = js::function<js_ui::dragint>;
    imgui_obj["dragint2"] = js::function<js_ui::dragint2>;
    imgui_obj["dragint3"] = js::function<js_ui::dragint3>;
    imgui_obj["dragint4"] = js::function<js_ui::dragint4>;
    imgui_obj["sliderfloat"] = js::function<js_ui::sliderfloat>;
    imgui_obj["sliderfloat2"] = js::function<js_ui::sliderfloat2>;
    imgui_obj["sliderfloat3"] = js::function<js_ui::sliderfloat3>;
    imgui_obj["sliderfloat4"] = js::function<js_ui::sliderfloat4>;
    imgui_obj["sliderangle"] = js::function<js_ui::sliderangle>;
    imgui_obj["sliderint"] = js::function<js_ui::sliderint>;
    imgui_obj["sliderint2"] = js::function<js_ui::sliderint2>;
    imgui_obj["sliderint3"] = js::function<js_ui::sliderint3>;
    imgui_obj["sliderint4"] = js::function<js_ui::sliderint4>;

    ///helpers for js reference semanticsitus
    imgui_obj["ref"] = js::function<js_ui::ref>;
    imgui_obj["get"] = js::function<js_ui::get>;

    js::add_key_value(global, "imgui", imgui_obj);

    /*#ifdef TESTING
    inject_c_function(ctx, hacky_get, "hacky_get", 0);
    #endif // TESTING*/

    ///console.log
    /*{
        js::value console(vctx);
        js::add_key_value(console, "log", js::function<async_print>);
        js::add_key_value(global, "console", console);
    }*/

    make_console_log(vctx);

    //#ifdef TESTING
    //if(seclevel <= 3)
    {
        dukx_setup_db_proxy(vctx);
    }
    //#endif // TESTING

    if(polyfill)
        dukx_inject_modules(vctx);
}
