#include "command_handler_state.hpp"

std::string command_handler_state::get_auth()
{
    std::lock_guard guard(command_lock);

    return auth;
}

void command_handler_state::set_auth(const std::string& str)
{
    std::lock_guard guard(command_lock);

    auth = str;
}

void command_handler_state::set_user(const user& usr)
{
    std::lock_guard guard(command_lock);

    current_user = usr;
}

user command_handler_state::get_user()
{
    std::lock_guard guard(command_lock);

    return current_user;
}

void command_handler_state::set_key_state(const std::string& str, bool is_down)
{
    if(str.size() > 10)
        return;

    std::lock_guard guard(key_lock);

    ///ur cheating!!!
    if(key_states.size() > 250)
        key_states.clear();

    key_states[str] = is_down;
}

std::map<std::string, bool> command_handler_state::get_key_state()
{
    std::lock_guard guard(key_lock);

    return key_states;
}
