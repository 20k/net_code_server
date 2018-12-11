#ifndef AUTH_HPP_INCLUDED
#define AUTH_HPP_INCLUDED

#include <string>
#include <vector>

#include "mongo.hpp"
#include <SFML/System.hpp>

struct mongo_lock_proxy;

///so the key thing to remember is
///every user and steam auth has a non steamauth
///but not every non steam auth will have a steam auth
struct auth
{
    bool valid = false;

    std::string auth_token_binary;
    std::string auth_token_hex;
    uint64_t steam_id = 0;
    std::vector<std::string> users;
    bool is_hex_encoding = false;

    bool load_from_db(mongo_lock_proxy& ctx, const std::string& auth_binary);
    bool load_from_db_steamid(mongo_lock_proxy& ctx, uint64_t psteam_id);
    void overwrite_in_db(mongo_lock_proxy& ctx);

    void insert_user_exclusive(const std::string& username);

    static void hacky_binary_conversion_check();
};

struct enforce_constant_time
{
    sf::Clock clk;

    ~enforce_constant_time();
};

template<typename T>
inline
void for_each_auth(const T& t)
{
    std::vector<mongo_requester> all;

    {
        mongo_lock_proxy all_auth = get_global_mongo_global_properties_context(-2);

        mongo_requester request;

        request.exists_check["account_token_hex"] = 1;

        all = request.fetch_from_db(all_auth);
    }

    for(auto& i : all)
    {
        t(i);
    }
}

#endif // AUTH_HPP_INCLUDED
