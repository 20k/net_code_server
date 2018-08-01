#ifndef COMMAND_HANDLER_HPP_INCLUDED
#define COMMAND_HANDLER_HPP_INCLUDED

#include <string>
#include <optional>

#include "user.hpp"
#include "seccallers.hpp"
#include "command_handler_state.hpp"
#include <memory>

struct shared_command_handler_state;

struct shared_data;
struct command_handler_state;
struct global_state;

///shared queue used for async responses from servers
std::string run_in_user_context(const std::string& username, const std::string& command, std::optional<std::shared_ptr<shared_command_handler_state>> all_shared, std::optional<float> custom_exec_time_s = std::nullopt, bool force_exec = false);
void throwaway_user_thread(const std::string& username, const std::string& command, std::optional<float> custom_exec_time_s = std::nullopt, bool force_exec = false);

///context?
std::string handle_command(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str);
void async_handle_command(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str);

std::string handle_autocompletes_json(const std::string& username, const std::string& in);

std::string binary_to_hex(const std::string& in, bool swap_endianness = false);
std::string hex_to_binary(const std::string& in);
std::string delete_user(command_handler_state& state, const std::string& str, bool cli_force = false);
std::string rename_user_force(const std::string& from_name, const std::string& to_name);

std::string handle_client_poll_json(user& usr);
void strip_old_msg_or_notif(mongo_lock_proxy& ctx);

namespace connection_type
{
    enum connection_type
    {
        WEBSOCKET,
        WEBSOCKET_SSL,
        HTTP
    };
}

using connection_t = connection_type::connection_type;

#endif // COMMAND_HANDLER_HPP_INCLUDED
