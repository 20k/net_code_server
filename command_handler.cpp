#include "command_handler.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"
#include <thread>
#include <chrono>
#include "http_beast_server.hpp"
#include "memory_sandbox.hpp"
#include "auth.hpp"
#include "logging.hpp"
#include <iomanip>
#include "rng.hpp"

struct unsafe_info
{
    user* usr;
    std::string command;
    duk_context* ctx;
    volatile int finished = 0;

    std::string ret;
};

inline
duk_ret_t unsafe_wrapper(duk_context* ctx, void* udata)
{
    unsafe_info* info = (unsafe_info*)udata;

    std::string ret = js_unified_force_call_data(info->ctx, info->command, info->usr->name);

    info->ret = ret;

    return 0;
}

void managed_duktape_thread(unsafe_info* info)
{
    if(duk_safe_call(info->ctx, unsafe_wrapper, (void*)info, 0, 1) != 0)
    {
        printf("Err in safe wrapper %s\n", duk_safe_to_string(info->ctx, -1));
    }

    duk_pop(info->ctx);

    info->finished = 1;
}

struct cleanup_auth_at_exit
{
    std::mutex& to_lock;
    std::map<std::string, int>& to_cleanup;
    std::string& auth;

    cleanup_auth_at_exit(std::mutex& lk, std::map<std::string, int>& cleanup, std::string& ath) : to_lock(lk), to_cleanup(cleanup), auth(ath) {}

    ~cleanup_auth_at_exit()
    {
        std::lock_guard<std::mutex> lk(to_lock);

        to_cleanup[auth] = 0;
    }
};

std::string run_in_user_context(const std::string& username, const std::string& command)
{
    user usr;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(-2);
        mongo_ctx->change_collection(username);

        usr.load_from_db(mongo_ctx, username);
    }

    static std::mutex id_mut;

    static std::map<std::string, int> auth_guard;
    static int32_t gthread_id = 0;
    int32_t local_thread_id;

    {
        std::lock_guard<std::mutex> lk(id_mut);

        local_thread_id = gthread_id++;

        if(auth_guard[usr.auth] == 1)
            return make_error_col("Cannot run two scripts at once in different contexts!");

        auth_guard[usr.auth] = 1;
    }

    cleanup_auth_at_exit cleanup(id_mut, auth_guard, usr.auth);

    stack_duk sd;
    //init_js_interop(sd, std::string());
    sd.ctx = create_sandbox_heap();
    native_register(sd.ctx);

    duk_memory_functions funcs;
    duk_get_memory_functions(sd.ctx, &funcs);

    sandbox_data* sand_data = (sandbox_data*)funcs.udata;

    fully_freeze(sd.ctx, "JSON", "Array", "parseInt", "parseFloat", "Math", "Date", "Error", "Number");

    startup_state(sd.ctx, usr.name, usr.name, "invoke");

    set_global_int(sd.ctx, "thread_id", local_thread_id);

    unsafe_info inf;
    inf.usr = &usr;
    inf.command = command;
    inf.ctx = sd.ctx;

    std::thread* launch = new std::thread(managed_duktape_thread, &inf);
    //launch->detach();

    bool terminated = false;

    //sf::Clock clk;
    float max_time_ms = 5000;
    float db_grace_time_ms = 1000;

    auto time_start = std::chrono::high_resolution_clock::now();

    #define ACTIVE_TIME_MANAGEMENT
    #ifdef ACTIVE_TIME_MANAGEMENT
    int active_time_slice_ms = 1;
    int sleeping_time_slice_ms = 1;
    #endif // ACTIVE_TIME_MANAGEMENT

    while(!inf.finished)
    {
        #ifdef ACTIVE_TIME_MANAGEMENT
        {
            Sleep(active_time_slice_ms);

            pthread_t thread = launch->native_handle();
            void* native_handle = pthread_gethandle(thread);
            SuspendThread(native_handle);
            Sleep(sleeping_time_slice_ms);
            ResumeThread(native_handle);
        }
        #endif // ACTIVE_TIME_MANAGEMENT

        auto time_current = std::chrono::high_resolution_clock::now();

        auto diff = time_current - time_start;

        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(diff);

        double elapsed = dur.count();

        if(elapsed >= max_time_ms + db_grace_time_ms)
        {
            pthread_t thread = launch->native_handle();

            void* native_handle = pthread_gethandle(thread);

            printf("UNSAFE THREAD TERMINATION\n");

            ///this is obviously very unsafe, doubly so due to the whole mutex thing, which may leave them locked
            ///going to need to have an intermittent sync point, where all threads block going in and we free all locks or something
            SuspendThread(native_handle);
            TerminateThread(native_handle, 1);
            CloseHandle(native_handle);

            inf.ret = "Ran for longer than " + std::to_string((int)max_time_ms) + "ms and was terminated";

            terminated = true;

            break;
        }

        if(elapsed >= max_time_ms)
        {
            sand_data->terminate_semi_gracefully = true;
        }

        Sleep(1);
    }

    if(inf.finished && !terminated)
    {
        launch->join();
        delete launch;
    }

    if(terminated)
    {
        for(auto& i : mongo_databases)
        {
            i->unlock_if(local_thread_id);
        }
    }

    //managed_duktape_thread(&inf);

    //if(!terminated)
    try
    {
        if(terminated)
            printf("Attempting unsafe resource cleanup\n");

        js_interop_shutdown(sd.ctx);
    }
    catch(...)
    {
        printf("Failed to cleanup resources\n");
    }

    printf("cleaned up unsafe\n");

    std::string ret = inf.ret;

    return ret;
}

void throwaway_user_thread(const std::string& username, const std::string& command)
{
    std::thread(run_in_user_context, username, command).detach();
}

std::string binary_to_hex(const std::string& in)
{
    std::string ret;

    const char* LUT = "0123456789ABCDEF";

    for(auto& i : in)
    {
        int lower_bits = ((int)i) & 0xF;
        int upper_bits = (((int)i) >> 4) & 0xF;

        ret += std::string(1, LUT[lower_bits]) + std::string(1, LUT[upper_bits]);
    }

    return ret;
}

int char_to_val(uint8_t c)
{
    if(c == '0')
        return 0;
    if(c == '1')
        return 1;
    if(c == '2')
        return 2;
    if(c == '3')
        return 3;
    if(c == '4')
        return 4;
    if(c == '5')
        return 5;
    if(c == '6')
        return 6;
    if(c == '7')
        return 7;
    if(c == '8')
        return 8;
    if(c == '9')
        return 9;
    if(c == 'A' || c == 'a')
        return 10;
    if(c == 'B' || c == 'b')
        return 11;
    if(c == 'C' || c == 'c')
        return 12;
    if(c == 'D' || c == 'd')
        return 13;
    if(c == 'E' || c == 'e')
        return 14;
    if(c == 'F' || c == 'f')
        return 15;

    return 0;
}

std::string hex_to_binary(const std::string& in)
{
    std::string ret;

    int len = in.size();

    for(int i=0; i < len; i+=2)
    {
        int next = i + 1;

        char cchar = in[i];
        char nchar = next < len ? in[next] : '0';

        int lower = char_to_val(cchar) + (char_to_val(nchar) << 4);

        ret.push_back(lower);
    }

    return ret;
}

void on_create_user(user& usr)
{
    throwaway_user_thread(usr.name, "#msg.manage({join:\"0000\"})");
    throwaway_user_thread(usr.name, "#msg.manage({join:\"7001\"})");
    throwaway_user_thread(usr.name, "#msg.manage({join:\"memes\"})");
}

std::string get_update_message()
{
    return "If you cannot login, a bad update deleted key.key files. PM me (20k) on discord with a username that you owned and I will recover it";
}

std::string handle_command_impl(command_handler_state& state, const std::string& str, global_state& glob, int64_t my_id)
{
    printf("yay command\n");

    //lg::log(str);

    if(starts_with(str, "user "))
    {
        if(state.auth == "")
            return make_error_col("Please create account with \"register client\"");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() != 2)
            return make_error_col("Invalid Command Error");

        std::string user_name = strip_whitespace(split_string[1]);

        if(!is_valid_string(user_name))
            return make_error_col("Invalid username");

        int32_t start_from = 0;

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(-2);

            mongo_requester request;
            request.set_prop("chats_send_is_gid", 1);

            std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

            if(found.size() >= 1)
                start_from = found[0].get_prop_as_integer("chats_send_gid");
            else
                printf("warning, no chats gid\n");
        }

        bool user_exists = false;

        {
            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

            if(state.current_user.exists(mongo_user_info, user_name))
            {
                user_exists = true;

                state.current_user.load_from_db(mongo_user_info, user_name);

                if(state.current_user.auth != state.auth)
                {
                    state.current_user = user();
                    return make_error_col("Incorrect Auth, someone else has registered this account or you are using a different pc and key.key file");
                }
            }
        }

        if(user_exists)
        {
            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(-2);

                auth to_check;
                to_check.load_from_db(mongo_ctx, state.auth);

                if(!to_check.valid)
                    return make_error_col("Trying something sneaky eh?");

                to_check.insert_user_exclusive(user_name);
                to_check.overwrite_in_db(mongo_ctx);
            }

            return "Switched to User";
        }
        else
        {
            state.current_user = user();

            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(-2);

                auth to_check;
                to_check.load_from_db(mongo_ctx, state.auth);

                if(!to_check.valid)
                    return make_error_col("Trying something sneaky eh 2?");

                #define MAX_USERS 8

                if(to_check.users.size() >= MAX_USERS)
                    return make_error_col("Max users " + std::to_string(to_check.users.size()) + "/" + std::to_string(MAX_USERS));

                to_check.insert_user_exclusive(user_name);
                to_check.overwrite_in_db(mongo_ctx);
            }

            {
                mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

                state.current_user.construct_new_user(mongo_user_info, user_name, state.auth, start_from);
                state.current_user.overwrite_user_in_db(mongo_user_info);
            }

            on_create_user(state.current_user);

            return make_success_col("Constructed new User");
        }
    }
    else if(starts_with(str, "#up ") || starts_with(str, "#dry ") || starts_with(str, "#up_es6 "))
    {
        if(state.auth == "")
            return make_error_col("No Auth");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 3)
        {
            if(starts_with(str, "#up "))
                return "Syntax is #up scriptname or invalid scriptname";
            if(starts_with(str, "#dry "))
                return "Syntax is #dry scriptname or invalid scriptname";
            if(starts_with(str, "#up_es6 ")) ///this is not client facing
                return "Syntax is #up scriptname or invalid scriptname";
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = state.current_user.name + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
        {
            return make_error_col("Invalid script name " + fullname);
        }

        auto begin_it = str.begin();

        int num_spaces = 0;

        while(num_spaces != 2 && begin_it != str.end())
        {
            if(*begin_it == ' ')
            {
                num_spaces++;
            }

            begin_it++;
        }

        bool is_es6 = starts_with(str, "#up_es6 ");

        if(begin_it != str.end())
        {
            std::string data_source(begin_it, str.end());

            bool was_public = false;

            {
                script_info script_inf;
                script_inf.name = fullname;

                mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(-2);
                script_inf.load_from_db(item_ctx);

                if(script_inf.valid && script_inf.in_public)
                    was_public = true;
            }

            stack_duk csd;
            csd.ctx = js_interop_startup();
            register_funcs(csd.ctx, 0);


            script_info script_inf;
            std::string compile_error = script_inf.load_from_unparsed_source(csd.ctx, data_source, fullname, is_es6);

            js_interop_shutdown(csd.ctx);

            if(compile_error != "")
                return compile_error;

            user cur;

            {
                mongo_lock_proxy user_locks = get_global_mongo_user_info_context(-2);

                cur.load_from_db(user_locks, state.current_user.name);
            }

            std::map<std::string, double> user_details;

            {
                mongo_lock_proxy items_lock = get_global_mongo_user_items_context(-2);
                user_details = cur.get_total_user_properties(items_lock);
            }

            int num_chars = script_inf.unparsed_source.size();
            int max_chars = user_details["char_count"];

            if(!starts_with(str, "#dry "))
            {
                script_inf.in_public = was_public;

                mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                script_inf.overwrite_in_db(mongo_ctx);
            }

            std::string rstr = "Upload Successful ";

            if(starts_with(str, "#dry "))
                rstr = "Dry Upload Successful ";

            return make_success_col(rstr + std::to_string(num_chars) + "/" + std::to_string(max_chars));
        }
    }
    else if(starts_with(str, "#remove "))
    {
        if(state.auth == "")
            return make_error_col("No Auth");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 2)
        {
            return "Syntax is #remove scriptname";
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = state.current_user.name + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
            return make_error_col("Invalid script name " + fullname);

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_info script_inf;
            script_inf.name = state.current_user.name + "." + scriptname;

            if(!script_inf.exists_in_db(mongo_ctx))
                return make_error_col("Script not found");

            mongo_requester request;
            request.set_prop("item_id", script_inf.name);

            request.remove_all_from_db(mongo_ctx);
        }

        return make_success_col("Script removed from server");
    }
    else if(starts_with(str, "#public ") || starts_with(str, "#private "))
    {
        if(state.auth == "")
            return make_error_col("No Auth");

        int in_public_state = starts_with(str, "#public ");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 2)
        {
            return "Syntax is #public scriptname or #private scriptname";
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = state.current_user.name + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
            return make_error_col("Invalid script name " + fullname);

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_info script_inf;
            script_inf.name = state.current_user.name + "." + scriptname;

            if(!script_inf.exists_in_db(mongo_ctx))
                return make_error_col("Script not found");

            mongo_requester request;
            request.set_prop("item_id", script_inf.name);

            mongo_requester to_set;
            to_set.set_prop("in_public", in_public_state);

            request.update_in_db_if_exact(mongo_ctx, to_set);
        }

        return make_success_col("Success");
    }
    #define ALLOW_SELF_AUTH
    #ifdef ALLOW_SELF_AUTH
    else if(starts_with(str, "register client"))
    {
        std::string to_ret = random_binary_string(128);

        mongo_requester request;
        request.set_prop_bin("account_token", to_ret);

        mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);
        request.insert_in_db(ctx);

        state.auth = to_ret;

        if(starts_with(str, "register client_hex"))
        {
            return "####registered secret_hex " + binary_to_hex(to_ret);
        }

        return "####registered secret " + to_ret;
    }
    #endif // ALLOW_SELF_AUTH
    else if(starts_with(str, "auth client ") || starts_with(str, "auth client_hex "))
    {
        printf("auth client\n");

        std::string which_str = "auth client ";

        if(starts_with(str, "auth client_hex "))
            which_str = "auth client_hex ";

        auto pos = str.begin() + which_str.size();;
        std::string auth_token = std::string(pos, str.end());

        if(starts_with(str, "auth client_hex "))
        {
            auth_token = hex_to_binary(auth_token);

            std::cout << "detected hex" << std::endl;
        }

        if(auth_token.length() > 140)
            return make_error_col("Auth too long");

        mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);

        mongo_requester request;
        request.set_prop_bin("account_token", auth_token);

        std::cout << "auth len " << auth_token.size() << std::endl;

        if(request.fetch_from_db(ctx).size() == 0)
            return make_error_col("Auth Failed, have you run \"register client\" at least once?");

        state.auth = auth_token;

        auth user_auth;

        user_auth.load_from_db(ctx, auth_token);

        std::vector<std::string> users = user_auth.users;

        std::string auth_string;

        for(auto& i : users)
        {
            auth_string += " " + colour_string(i);
        }

        std::string full_string = "Users Found:";

        if(auth_string == "")
            full_string = "No Users Found. Type user <username> to register";

        std::cout << auth_string << std::endl;

        return make_success_col("Auth Success") + "\n" + full_string + auth_string + "\n" + get_update_message();
    }
    else if(starts_with(str, "auth client") || starts_with(str, "auth client_hex"))
    {
        return "No Auth, send \"register client\"";
    }
    else
    {
        {
            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

            if(!state.current_user.exists(mongo_user_info, state.current_user.name))
                return "No account or not logged in";
        }

        return run_in_user_context(state.current_user.name, str);
    }

    return make_error_col("Command Not Found or Unimplemented");
}

std::vector<mongo_requester> get_and_update_notifs_for_user(user& usr)
{
    std::vector<mongo_requester> found;

    {
        mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
        ctx->change_collection(usr.name);

        mongo_requester to_send;
        //to_send.set_prop("to_user", usr.name);
        to_send.set_prop("is_chat", 1);
        to_send.set_prop("processed", 0);

        found = to_send.fetch_from_db(ctx);

        mongo_requester old_search = to_send;

        to_send.set_prop("processed", 1);

        old_search.update_in_db_if_exact(ctx, to_send);

        //to_send.remove_all_from_db(ctx);
    }

    if(found.size() > 1000)
        found.resize(1000);

    return found;
}

std::vector<std::string> get_channels_for_user(user& usr)
{
    mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
    ctx->change_collection(usr.name);

    mongo_requester request;
    request.set_prop("name", usr.name);

    auto mfound = request.fetch_from_db(ctx);

    if(mfound.size() != 1)
        return {};

    mongo_requester& cur_user = mfound[0];

    return str_to_array(cur_user.get_prop("joined_channels"));
}

std::string handle_client_poll(user& usr)
{
    std::vector<mongo_requester> found = get_and_update_notifs_for_user(usr);

    std::vector<std::string> channels = get_channels_for_user(usr);

    std::string to_send = "";

    std::string prologue_str = std::to_string(channels.size()) + " " + array_to_str(channels);

    while(prologue_str.size() > 0 && prologue_str.back() == ' ')
        prologue_str.pop_back();

    prologue_str = prologue_str + " ";

    to_send = std::to_string(prologue_str.size()) + " " + prologue_str;

    for(mongo_requester& req : found)
    {
        std::string chan = req.get_prop("channel");

        std::vector<mongo_requester> to_col{req};

        std::string full_str = chan + " " + prettify_chat_strings(to_col);

        to_send += std::to_string(full_str.size()) + " " + full_str;

        //to_send += "chat_api " + std::to_string(full_str.size()) + " " + full_str;
    }

    //std::cout << to_send << std::endl;

    if(to_send == "")
        return "";

    return "chat_api " + to_send;
}

std::string handle_client_poll_json(user& usr)
{
    std::vector<mongo_requester> found = get_and_update_notifs_for_user(usr);

    duk_context* ctx = js_interop_startup();

    duk_object_t to_encode;

    std::vector<std::string> channels = get_channels_for_user(usr);

    to_encode["channels"] = channels;

    std::vector<duk_placeholder_t> objects;

    for(mongo_requester& req : found)
    {
        duk_object_t obj;

        std::string chan = req.get_prop("channel");
        std::vector<mongo_requester> to_col{req};
        std::string pretty = prettify_chat_strings(to_col);

        ///so, wanna encode {channel:"chan", pretty:"pretty"}

        obj["channel"] = chan;
        obj["text"] = pretty;

        objects.push_back(new duk_object_t(obj));
    }

    to_encode["data"] = objects;

    push_duk_val(ctx, to_encode);

    const char* ptr = duk_json_encode(ctx, -1);

    std::string str;

    if(ptr != nullptr)
    {
        str = std::string(ptr);
    }

    duk_pop(ctx);

    js_interop_shutdown(ctx);

    for(auto& i : objects)
        delete (duk_object_t*)i;

    return "chat_api_json " + str;
}

///needs to handle script bundles
///use unified script loading
std::optional<std::vector<script_arg>> get_uniform_script_args(user& usr, const std::string& script)
{
    if(privileged_args.find(script) != privileged_args.end())
    {
        return privileged_args[script];
    }

    std::string err;

    unified_script_info script_inf = unified_script_loading(usr.name, -2, script, err);

    if(!script_inf.valid)
        return std::nullopt;

    if(script_inf.args.size() != script_inf.params.size())
        return std::nullopt;

    std::vector<script_arg> args;

    for(int i=0; i < (int)script_inf.args.size(); i++)
    {
        args.push_back({script_inf.args[i], script_inf.params[i]});
    }

    return args;
}

std::string handle_autocompletes(user& usr, const std::string& in)
{
    std::vector<std::string> dat = no_ss_split(in, " ");

    if(dat.size() < 2)
        return "server_scriptargs_invalid";

    std::string script = dat[1];

    if(!is_valid_full_name_string(script))
        return "server_scriptargs_invalid " + script;

    if(SHOULD_RATELIMIT(usr.name, AUTOCOMPLETES))
        return "server_scriptargs_ratelimit " + script;

    auto opt_arg = get_uniform_script_args(usr, script);

    if(!opt_arg.has_value())
        return "server_scriptargs_invalid " + script;

    auto args = *opt_arg;

    std::string intro = "server_scriptargs " + std::to_string(script.size()) + " " + script + " ";

    std::string ret;

    for(script_arg& arg : args)
    {
        ret += std::to_string(arg.key.size()) + " " + arg.key + " ";
        ret += std::to_string(arg.val.size()) + " " + arg.val + " ";
    }

    ///if!public && not owned by me
    ///return nothing

    return intro + ret;
}

std::string handle_autocompletes_json(user& usr, const std::string& in)
{
    std::vector<std::string> dat = no_ss_split(in, " ");

    if(dat.size() < 2)
        return "server_scriptargs_invalid_json";

    std::string script = dat[1];

    if(!is_valid_full_name_string(script))
        return "server_scriptargs_invalid_json " + script;

    duk_object_t obj;
    obj["script"] = script;

    if(SHOULD_RATELIMIT(usr.name, AUTOCOMPLETES))
        return "server_scriptargs_ratelimit_json " + dukx_json_get(obj);

    auto opt_arg = get_uniform_script_args(usr, script);

    if(!opt_arg.has_value())
        return "server_scriptargs_invalid_json " + script;

    auto args = *opt_arg;

    std::string intro = "server_scriptargs_json ";

    std::vector<std::string> keys;
    std::vector<std::string> vals;

    for(script_arg& arg : args)
    {
        keys.push_back(arg.key);
        vals.push_back(arg.val);
    }

    obj["keys"] = keys;
    obj["vals"] = vals;

    std::string rep = dukx_json_get(obj);

    return intro + rep;
}

std::string handle_command(command_handler_state& state, const std::string& str, global_state& glob, int64_t my_id)
{
    //lg::log("Log Command " + str);

    std::string client_command = "client_command ";
    std::string client_chat = "client_chat ";
    std::string client_poll = "client_poll";
    std::string client_poll_json = "client_poll_json";

    std::string client_scriptargs = "client_scriptargs ";
    std::string client_scriptargs_json = "client_scriptargs_json ";

    if(starts_with(str, client_command))
    {
        std::string to_exec(str.begin() + client_command.size(), str.end());

        return "command " + handle_command_impl(state, to_exec, glob, my_id);
    }

    if(starts_with(str, client_chat))
    {
        std::string to_exec(str.begin() + client_chat.size(), str.end());

        handle_command_impl(state, to_exec, glob, my_id);

        return "";
    }

    ///matches both client poll and json
    if(starts_with(str, client_poll))
    {
        if(state.auth == "" || state.current_user.name == "")
            return "";

        {
            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

            if(!state.current_user.exists(mongo_user_info, state.current_user.name))
                return "";

            state.current_user.load_from_db(mongo_user_info, state.current_user.name);
        }

        if(starts_with(str, client_poll_json))
            return handle_client_poll_json(state.current_user);
        if(starts_with(str, client_poll))
            return handle_client_poll(state.current_user);
    }

    if(starts_with(str, client_scriptargs))
    {
        if(state.auth == "" || state.current_user.name == "")
            return "";

        return handle_autocompletes(state.current_user, str);
    }

    if(starts_with(str, client_scriptargs_json))
    {
        if(state.auth == "" || state.current_user.name == "")
            return "";

        return handle_autocompletes_json(state.current_user, str);
    }

    return "command Command not understood";
}
