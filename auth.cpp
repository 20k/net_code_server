#include "auth.hpp"
#include "mongo.hpp"
#include <libncclient/nc_util.hpp>
#include "command_handler.hpp"

///perform conversion of all auth tokens to base 64 so we can ditch mongos binary format

void load_from_request(auth& ath, mongo_requester& i)
{
    if(i.has_prop("account_token_hex"))
    {
        ath.auth_token_hex = i.get_prop("account_token_hex");
        ath.auth_token_binary = hex_to_binary(ath.auth_token_hex);

        ath.valid = true;
    }

    if(i.has_prop("users"))
    {
        ath.users =  (std::vector<std::string>)i.get_prop("users");
    }

    if(i.has_prop("is_hex_encoding"))
    {
        ath.is_hex_encoding = i.get_prop_as_integer("is_hex_encoding");
    }

    if(i.has_prop("steam_id"))
    {
        ath.steam_id = i.get_prop_as_uinteger("steam_id");
    }
}

bool auth::load_from_db(mongo_lock_proxy& ctx, const std::string& auth_binary_in)
{
    mongo_requester request;
    request.set_prop("account_token_hex", binary_to_hex(auth_binary_in));

    std::vector<mongo_requester> found = request.fetch_from_db(ctx);

    if(found.size() != 1)
    {
        //printf("Invalid user auth token\n");

        return false;
    }

    for(mongo_requester& i : found)
    {
        load_from_request(*this, i);
    }

    return valid;
}

bool auth::load_from_db_steamid(mongo_lock_proxy& ctx, uint64_t psteam_id)
{
    steam_id = psteam_id;

    mongo_requester request;
    request.set_prop("steam_id", psteam_id);

    std::vector<mongo_requester> found = request.fetch_from_db(ctx);

    if(found.size() != 1)
    {
        //printf("Invalid user auth token\n");

        return false;
    }

    for(mongo_requester& i : found)
    {
        load_from_request(*this, i);
    }

    return valid;
}

void auth::overwrite_in_db(mongo_lock_proxy& ctx)
{
    if(!valid)
        return;

    mongo_requester request;
    request.set_prop("account_token_hex", auth_token_hex);

    mongo_requester to_set;
    to_set.set_prop("users", users);
    to_set.set_prop("is_hex_encoding", is_hex_encoding);
    to_set.set_prop("steam_id", steam_id);

    request.update_in_db_if_exact(ctx, to_set);
}

void auth::insert_user_exclusive(const std::string& username)
{
    for(auto& i : users)
    {
        if(i == username)
            return;
    }

    users.push_back(username);
}

enforce_constant_time::~enforce_constant_time()
{
    while(clk.getElapsedTime().asMicroseconds() < 100 * 1000){}
}
