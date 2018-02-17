#include "command_handler.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"

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
            register_funcs(csd.ctx);

            script_info script_inf;
            std::string compile_error = script_inf.load_from_unparsed_source(csd.ctx, data_source, fullname);
            script_inf.overwrite_in_db();

            js_interop_shutdown(csd.ctx);

            if(compile_error != "")
                return compile_error;

            return "Uploaded Successfully";
        }
    }

    return "Command Not Found or Unimplemented";
}
