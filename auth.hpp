#ifndef AUTH_HPP_INCLUDED
#define AUTH_HPP_INCLUDED

#include <string>
#include <vector>
#include <SFML/System.hpp>
#include <networking/serialisable_fwd.hpp>
#include "db_storage_backend_lmdb.hpp"
#include <toolkit/clock.hpp>

///so the key thing to remember is
///every user and steam auth has a non steamauth
///but not every non steam auth will have a steam auth
struct auth : serialisable, free_function
{
    std::string auth_token_hex;
    uint64_t steam_id = 0;
    std::vector<std::string> users;
    bool is_free_account = false;

    bool load_from_db(db::read_tx& ctx, const std::string& auth_binary);
    bool load_from_db_steamid(db::read_tx& ctx, uint64_t psteam_id);
    void overwrite_in_db(db::read_write_tx& ctx);

    void insert_user_exclusive(const std::string& username);
    bool contains_user(const std::string& username);
};

struct enforce_constant_time
{
    steady_timer clk;

    ~enforce_constant_time();
};

#endif // AUTH_HPP_INCLUDED
