#include "command_handler.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"
#include <thread>
#include <chrono>
#include <Wincrypt.h>

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

inline
std::string run_in_user_context(user& usr, const std::string& command)
{
    static std::mutex id_mut;

    static int32_t gthread_id = 0;
    int32_t local_thread_id;

    {
        std::lock_guard<std::mutex> lk(id_mut);

        local_thread_id = gthread_id++;
    }

    stack_duk sd;
    init_js_interop(sd, std::string());

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

    auto time_start = std::chrono::high_resolution_clock::now();

    while(!inf.finished)
    {
        auto time_current = std::chrono::high_resolution_clock::now();

        auto diff = time_current - time_start;

        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(diff);

        double elapsed = dur.count();

        if(elapsed >= max_time_ms)
        {
            pthread_t thread = launch->native_handle();

            void* native_handle = pthread_gethandle(thread);

            ///this is obviously very unsafe, doubly so due to the whole mutex thing, which may leave them locked
            ///going to need to have an intermittent sync point, where all threads block going in and we free all locks or something
            SuspendThread(native_handle);
            TerminateThread(native_handle, 1);
            CloseHandle(native_handle);

            inf.ret = "Ran for longer than " + std::to_string((int)max_time_ms) + "ms and was terminated";

            terminated = true;

            break;
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

    if(!terminated)
        js_interop_shutdown(sd.ctx);

    std::string ret = inf.ret;

    return ret;
}

bool starts_with(const std::string& in, const std::string& test)
{
    if(in.substr(0, test.length()) == test)
        return true;

    return false;
}

std::string handle_command(command_handler_state& state, const std::string& str)
{
    printf("yay command\n");

    std::cout << str << std::endl;

    if(starts_with(str, "user "))
    {
        if(state.auth == "")
            return "Please create account with \"register client\"";

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() != 2)
            return "Invalid Command Error";

        std::string user = strip_whitespace(split_string[1]);

        if(!is_valid_string(user))
        {
            return "Invalid username";
        }

        mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

        if(state.current_user.exists(mongo_user_info, user))
        {
            std::cout << "exist\n";

            state.current_user.load_from_db(mongo_user_info, user);

            std::cout << "AUTH " << state.current_user.auth.size() << std::endl;
            std::cout << "FAUTH " << state.auth.size() << std::endl;

            if(state.current_user.auth != state.auth)
                return "Incorrect Auth";

            return "Switched to User";
        }
        else
        {
            state.current_user.construct_new_user(mongo_user_info, user, state.auth);
            state.current_user.overwrite_user_in_db(mongo_user_info);

            return "Constructed new User";
        }
    }
    else if(starts_with(str, "#up "))
    {
        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 3)
        {
            return "Syntax is #up scriptname";
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = state.current_user.name + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
        {
            return "Invalid script name " + fullname;
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

            return "Uploaded Successfully";
        }
    }
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
        request.set_prop("account_token", to_ret);

        mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);
        request.insert_in_db(ctx);

        state.auth = to_ret;

        return "####registered secret " + to_ret;
    }
    else if(starts_with(str, "auth client "))
    {
        printf("auth client\n");
        std::cout << str << std::endl;

        auto pos = str.begin() + strlen("auth client ");
        std::string auth = std::string(pos, str.end());

        state.auth = auth;

        return "Auth Success";
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

    return "Command Not Found or Unimplemented";
}
