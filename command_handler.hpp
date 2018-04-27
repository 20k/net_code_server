#ifndef COMMAND_HANDLER_HPP_INCLUDED
#define COMMAND_HANDLER_HPP_INCLUDED

#include <string>

#include "user.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"

///will need account here as well

inline
void init_js_interop(stack_duk& sd, const std::string& js_data)
{
    sd.ctx = js_interop_startup();
}

std::string run_in_user_context(const std::string& username, const std::string& command);
void throwaway_user_thread(const std::string& username, const std::string& command);

struct command_handler_state
{
    std::mutex lock;

    user current_user;

    std::string auth;
};

struct global_state;

///context?
std::string handle_command(command_handler_state& state, const std::string& str, global_state& glob, int64_t my_id);

std::string handle_autocompletes_json(user& usr, const std::string& in);

std::string binary_to_hex(const std::string& in);
std::string hex_to_binary(const std::string& in);
std::string delete_user(command_handler_state& state, const std::string& str, bool cli_force = false);

#endif // COMMAND_HANDLER_HPP_INCLUDED
