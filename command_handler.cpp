#include "command_handler.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"

struct unsafe_info
{
    user* usr;
    std::string command;
    duk_context* ctx;

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

inline
std::string run_in_user_context(user& usr, const std::string& command)
{
    stack_duk sd;
    init_js_interop(sd, std::string());

    fully_freeze(sd.ctx, "JSON", "Array", "parseInt", "parseFloat", "Math", "Date", "Error", "Number");

    startup_state(sd.ctx, usr.name, usr.name, "invoke");

    unsafe_info inf;
    inf.usr = &usr;
    inf.command = command;
    inf.ctx = sd.ctx;

    if(duk_safe_call(sd.ctx, unsafe_wrapper, (void*)&inf, 0, 1) != 0)
    {
        printf("Err in safe wrapper %s\n", duk_safe_to_string(sd.ctx, -1));
    }

    duk_pop(sd.ctx);

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

    if(starts_with(str, "user "))
    {
        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() != 2)
            return "Invalid Command Error";

        std::string user = split_string[1];

        mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context();

        if(state.current_user.exists(mongo_user_info, user))
        {
            std::cout << "exist\n";

            state.current_user.load_from_db(mongo_user_info, user);

            return "Switched to User";
        }
        else
        {
            state.current_user.construct_new_user(mongo_user_info, user);
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

        std::string scriptname = split_string[1];

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

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context();

            script_inf.overwrite_in_db(mongo_ctx);

            js_interop_shutdown(csd.ctx);

            if(compile_error != "")
                return compile_error;

            return "Uploaded Successfully";
        }
    }
    else
    {
        mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context();

        if(state.current_user.exists(mongo_user_info, state.current_user.name))
        {
            std::string ret = run_in_user_context(state.current_user, str);

            return ret;
        }
        else
        {
            return "No account or not logged in";
        }
    }

    return "Command Not Found or Unimplemented";
}
