#include "command_handler.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"
#include <thread>
#include <chrono>
#include <Wincrypt.h>
#include "http_beast_server.hpp"
#include "memory_sandbox.hpp"
#include "auth.hpp"
#include "logging.hpp"
#include <iomanip>

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

std::string handle_command_impl(command_handler_state& state, const std::string& str, global_state& glob, int64_t my_id)
{
    printf("yay command\n");

    lg::log(str);

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

        mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

        if(state.current_user.exists(mongo_user_info, user_name))
        {
            state.current_user.load_from_db(mongo_user_info, user_name);

            if(state.current_user.auth != state.auth)
            {
                state.current_user = user();
                return make_error_col("Incorrect Auth");
            }

            ///WARNING NESTED LOCKS
            ///this is actually a fallback case
            ///in case a user was created under the old system
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

            state.current_user.construct_new_user(mongo_user_info, user_name, state.auth, start_from);
            state.current_user.overwrite_user_in_db(mongo_user_info);

            return make_success_col("Constructed new User");
        }
    }
    else if(starts_with(str, "#up "))
    {
        if(state.auth == "")
            return make_error_col("No Auth");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 3)
        {
            return "Syntax is #up scriptname";
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

        if(begin_it != str.end())
        {
            std::string data_source(begin_it, str.end());

            stack_duk csd;
            csd.ctx = js_interop_startup();
            register_funcs(csd.ctx, 0);

            script_info script_inf;
            std::string compile_error = script_inf.load_from_unparsed_source(csd.ctx, data_source, fullname);

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

            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                script_inf.overwrite_in_db(mongo_ctx);
            }

            return make_success_col("Upload Successful " + std::to_string(num_chars) + "/" + std::to_string(max_chars));
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
        unsigned char random_bytes[128] = {0};

        HCRYPTPROV provider = 0;

        if(!CryptAcquireContext(&provider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
            return "Nope";

        if(!CryptGenRandom(provider, 128, random_bytes))
            return "Nope2.0";

        CryptReleaseContext(provider, 0);

        std::string to_ret;
        to_ret.resize(128);

        for(int i=0; i < 128; i++)
        {
            to_ret[i] = random_bytes[i];
        }

        mongo_requester request;
        request.set_prop_bin("account_token", to_ret);

        mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);
        request.insert_in_db(ctx);

        state.auth = to_ret;

        if(starts_with(str, "register client_hex"))
        {
            return "####registered secret " + binary_to_hex(to_ret);
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

        return make_success_col("Auth Success") + "\n" + full_string + auth_string;
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

std::string handle_client_poll(user& usr)
{
    std::vector<mongo_requester> found;

    int32_t start_from = usr.last_message_uid;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channels_context(-2);

        mongo_requester request;
        request.gt_than_i["uid"] = start_from;
        //request.lt_than["uid"] = stringify_hack(999);

        found = request.fetch_from_db(mongo_ctx);
    }

    //std::cout << "poll\n";

    if(found.size() == 0)
        return "";

    std::cout << "found num " << found.size() << std::endl;

    int64_t last_uid = start_from;

    std::map<std::string, std::vector<mongo_requester>> channel_map;
    std::map<std::string, std::string> channel_to_string;

    for(auto& i : found)
    {
        channel_map[i.get_prop("channel")].push_back(i);

        if(i.get_prop_as_integer("uid") > last_uid)
        {
            last_uid = i.get_prop_as_integer("uid");
        }
    }

    usr.last_message_uid = last_uid;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(-2);

        usr.overwrite_user_in_db(mongo_ctx);

        std::cout << "started @ " << start_from << " ended at " << last_uid << std::endl;
    }

    for(auto& i : channel_map)
    {
        std::sort(i.second.begin(), i.second.end(), [](mongo_requester& i1, mongo_requester& i2){return i1.get_prop("uid") > i2.get_prop("uid");});
    }

    int max_chat_dump = 1000;

    for(auto& i : channel_map)
    {
        if((int)i.second.size() > max_chat_dump)
            i.second.resize(max_chat_dump);

        channel_to_string[i.first] = prettify_chat_strings(i.second);
    }

    std::string to_send = "";

    if(channel_to_string.size() != 0)
    {
        for(auto& cdata : channel_to_string)
        {
            std::string full_str = cdata.first + " " + cdata.second;

            to_send += "chat_api " + std::to_string(full_str.size()) + " " + full_str;
        }
    }

    if(to_send.size() == 0)
        return "";

    return to_send;
}

std::string handle_command(command_handler_state& state, const std::string& str, global_state& glob, int64_t my_id)
{
    std::string client_command = "client_command ";
    std::string client_chat = "client_chat ";
    std::string client_poll = "client_poll";

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

        return handle_client_poll(state.current_user);
    }

    return "command Command not understood";
}
