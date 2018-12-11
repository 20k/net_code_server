#ifndef STEAM_AUTH_HPP_INCLUDED
#define STEAM_AUTH_HPP_INCLUDED

#include <memory>
#include <optional>
#include <stdint.h>

std::optional<uint64_t> get_steam_auth(const std::string& hex_auth_data);

#endif // STEAM_AUTH_HPP_INCLUDED
