#include "auth.hpp"
#include "mongo.hpp"
#include <libncclient/nc_util.hpp>

///perform conversion of all auth tokens to base 64 so we can ditch mongos binary format

bool auth::load_from_db(mongo_lock_proxy& ctx, const std::string& auth_binary_in)
{
    mongo_requester request;
    request.set_prop_bin("account_token", auth_binary_in);

    std::vector<mongo_requester> found = request.fetch_from_db(ctx);

    if(found.size() != 1)
    {
        //printf("Invalid user auth token\n");

        return false;
    }

    for(mongo_requester& i : found)
    {
        if(i.has_prop("account_token"))
        {
            auth_token_binary = i.get_prop("account_token");
            valid = true;
        }

        if(i.has_prop("users"))
        {
            std::string user_string = i.get_prop("users");

            ///yeah kinda dumb
            users = no_ss_split(user_string, " ");
        }

        if(i.has_prop("is_hex_encoding"))
        {
            is_hex_encoding = i.get_prop_as_integer("is_hex_encoding");
        }
    }

    return valid;
}

void auth::overwrite_in_db(mongo_lock_proxy& ctx)
{
    if(!valid)
        return;

    mongo_requester request;
    request.set_prop_bin("account_token", auth_token_binary);

    std::string accum;

    for(auto& i : users)
    {
        accum += i + " ";
    }

    if(accum.size() == 0)
        return;

    ///its a space
    accum.pop_back();

    mongo_requester to_set;
    to_set.set_prop("users", accum);
    to_set.set_prop("is_hex_encoding", is_hex_encoding);

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

void auth::hacky_binary_conversion_check()
{
    for_each_auth([](mongo_requester& req)
                  {

                  });
}
