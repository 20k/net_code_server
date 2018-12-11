#ifndef STEAM_AUTH_HPP_INCLUDED
#define STEAM_AUTH_HPP_INCLUDED

#include <memory>
#include <optional>
#include <stdint.h>

struct shared_command_handler_state;

std::optional<uint64_t> get_steam_auth(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& hex_auth_data);

#endif // STEAM_AUTH_HPP_INCLUDED
