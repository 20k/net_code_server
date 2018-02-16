#ifndef COMMAND_HANDLER_HPP_INCLUDED
#define COMMAND_HANDLER_HPP_INCLUDED

#include <string>

#include "user.hpp"

///will need account here as well

struct command_handler_state
{
    user current_user;
};

///context?
std::string handle_command(command_handler_state& state, const std::string& str);

#endif // COMMAND_HANDLER_HPP_INCLUDED
