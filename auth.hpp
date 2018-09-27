#ifndef AUTH_HPP_INCLUDED
#define AUTH_HPP_INCLUDED

#include <string>
#include <vector>

#include "mongo.hpp"

struct mongo_lock_proxy;

struct auth
{
    bool valid = false;

    std::string auth_token_binary;
    std::string auth_token_hex;
    std::vector<std::string> users;
    bool is_hex_encoding = false;

    bool load_from_db(mongo_lock_proxy& ctx, const std::string& auth_binary);
    void overwrite_in_db(mongo_lock_proxy& ctx);

    void insert_user_exclusive(const std::string& username);

    static void hacky_binary_conversion_check();
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
