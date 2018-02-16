#include "command_handler.hpp"

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

    return "Command Not Found or Unimplemented";
}
