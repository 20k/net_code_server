#ifndef STEAM_AUTH_HPP_INCLUDED
#define STEAM_AUTH_HPP_INCLUDED

#include <memory>
#include <optional>
#include <stdint.h>
#include <string>

struct steam_auth_data
{
    uint64_t steam_id = 0;
    std::string user_data;
};

std::optional<steam_auth_data> get_steam_auth(const std::string& hex_auth_data);

#endif // STEAM_AUTH_HPP_INCLUDED
