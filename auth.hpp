#ifndef AUTH_HPP_INCLUDED
#define AUTH_HPP_INCLUDED

#include <string>
#include <vector>

struct mongo_lock_proxy;

struct auth
{
    bool valid = false;

    std::string auth_token;
    std::vector<std::string> users;

    void load_from_db(mongo_lock_proxy& ctx, const std::string& auth);
    void overwrite_in_db(mongo_lock_proxy& ctx);

    void insert_user_exclusive(const std::string& username);
};

#endif // AUTH_HPP_INCLUDED
