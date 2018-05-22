#ifndef COMMAND_HANDLER_HPP_INCLUDED
#define COMMAND_HANDLER_HPP_INCLUDED

#include <string>
#include <optional>

#include "user.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"
#include "command_handler_state.hpp"
#include <memory>

struct shared_command_handler_state;

struct shared_data;
struct command_handler_state;
struct global_state;

inline
void init_js_interop(stack_duk& sd, const std::string& js_data)
{
    sd.ctx = js_interop_startup();
}

///shared queue used for async responses from servers
std::string run_in_user_context(const std::string& username, const std::string& command, std::optional<std::shared_ptr<shared_command_handler_state>> all_shared);
void throwaway_user_thread(const std::string& username, const std::string& command);

///context?
std::string handle_command(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str);

std::string handle_autocompletes_json(const std::string& username, const std::string& in);

std::string binary_to_hex(const std::string& in, bool swap_endianness = false);
std::string hex_to_binary(const std::string& in);
std::string delete_user(command_handler_state& state, const std::string& str, bool cli_force = false);
std::string rename_user_force(const std::string& from_name, const std::string& to_name);

namespace connection_type
{
    enum connection_type
    {
        WEBSOCKET,
        HTTP
    };
}

using connection_t = connection_type::connection_type;

#endif // COMMAND_HANDLER_HPP_INCLUDED
