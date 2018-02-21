#include "command_handler.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"
#include <thread>
#include <chrono>
#include <Wincrypt.h>
#include "http_beast_server.hpp"
#include "memory_sandbox.hpp"

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

inline
std::string run_in_user_context(user& usr, const std::string& command)
{
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

    fully_freeze(sd.ctx, "JSON", "Array", "parseInt", "parseFloat", "Math", "Date", "Error", "Number", "print", "sleep");

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
        std::lock_guard<std::mutex> lk(mongo_databases_lock);

        for(auto& i : mongo_databases)
        {
            i.second->unlock_if(local_thread_id);
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

    std::string ret = inf.ret;

    return ret;
}

bool starts_with(const std::string& in, const std::string& test)
{
    if(in.substr(0, test.length()) == test)
        return true;

    return false;
}

std::string handle_command_impl(command_handler_state& state, const std::string& str, global_state& glob, int64_t my_id)
{
    printf("yay command\n");

    if(starts_with(str, "user "))
    {
        if(state.auth == "")
            return make_error_col("Please create account with \"register client\"");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() != 2)
            return make_error_col("Invalid Command Error");

        std::string user = strip_whitespace(split_string[1]);

        if(!is_valid_string(user))
            return make_error_col("Invalid username");

        mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

        if(state.current_user.exists(mongo_user_info, user))
        {
            state.current_user.load_from_db(mongo_user_info, user);

            if(state.current_user.auth != state.auth)
                return make_error_col("Incorrect Auth");

            return "Switched to User";
        }
        else
        {
            state.current_user.construct_new_user(mongo_user_info, user, state.auth);
            state.current_user.overwrite_user_in_db(mongo_user_info);

            return make_success_col("Constructed new User");
        }
    }
    else if(starts_with(str, "#up "))
    {
        if(state.auth == "")
            return make_error_col("No auth");

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

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_inf.overwrite_in_db(mongo_ctx);

            js_interop_shutdown(csd.ctx);

            if(compile_error != "")
                return compile_error;

            return make_success_col("Uploaded Successfully");
        }
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
            return "Nope";

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

        return "####registered secret " + to_ret;
    }
    #endif // ALLOW_SELF_AUTH
    else if(starts_with(str, "auth client "))
    {
        printf("auth client\n");
        //std::cout << str << std::endl;

        auto pos = str.begin() + strlen("auth client ");
        std::string auth = std::string(pos, str.end());

        mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);

        mongo_requester request;
        request.set_prop_bin("account_token", auth);

        std::cout << "auth len " << auth.size() << std::endl;

        if(request.fetch_from_db(ctx).size() == 0)
            return make_error_col("Auth Failed");

        //std::lock_guard<std::mutex> lk(glob.auth_lock);

        #if 0
        ///so if we reauth but on the same thread that's fine
        ///or if we auth and we haven't authed on any thread before
        if(glob.auth_locks[auth] != my_id && glob.auth_locks[auth] != 0)
            return make_error_col("Oh boy this better be a random error otherwise ur getting banned.\nThis is a joke but seriously don't do this");
        #endif // 0

        state.auth = auth;

        //glob.auth_locks[state.auth] = my_id;

        return make_success_col("Auth Success");
    }
    else if(starts_with(str, "auth client"))
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

        return run_in_user_context(state.current_user, str);
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
        if(i.second.size() > max_chat_dump)
            i.second.resize(max_chat_dump);

        channel_to_string[i.first] = prettify_chat_strings(i.second);
    }

    std::string to_send = "";

    if(channel_to_string.size() != 0)
    {
        for(auto& cdata : channel_to_string)
        {
            to_send += cdata.first + " " + cdata.second;
        }
    }

    if(to_send.size() == 0)
        return "";

    return "chat_api " + to_send;
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

        //int uid =
    }

    return "command Command not understood";
}
