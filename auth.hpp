#ifndef AUTH_HPP_INCLUDED
#define AUTH_HPP_INCLUDED

#include <string>
#include <vector>
#include <SFML/System.hpp>
#include <networking/serialisable_fwd.hpp>

struct mongo_lock_proxy;

///so the key thing to remember is
///every user and steam auth has a non steamauth
///but not every non steam auth will have a steam auth
struct auth : serialisable, free_function
{
    std::string auth_token_binary;
    std::string auth_token_hex;
    uint64_t steam_id = 0;
    std::vector<std::string> users;

    bool load_from_db(mongo_lock_proxy& ctx, const std::string& auth_binary);
    bool load_from_db_steamid(mongo_lock_proxy& ctx, uint64_t psteam_id);
    void overwrite_in_db(mongo_lock_proxy& ctx);

    void insert_user_exclusive(const std::string& username);
};

struct enforce_constant_time
{
    sf::Clock clk;

    ~enforce_constant_time();
};

#endif // AUTH_HPP_INCLUDED
