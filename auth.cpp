#include "auth.hpp"
#include "mongo.hpp"
#include <libncclient/nc_util.hpp>
#include "command_handler.hpp"
#include <networking/serialisable.hpp>
#include "serialisables.hpp"

bool auth::load_from_db(mongo_lock_proxy& ctx, const std::string& auth_binary_in)
{
    nlohmann::json req;
    req["auth_token_hex"] = binary_to_hex(auth_binary_in);

    std::vector<nlohmann::json> found = fetch_from_db(ctx, req);

    if(found.size() != 1)
        return false;

    *this = auth();

    deserialise(found[0], *this, serialise_mode::DISK);

    return true;
}

bool auth::load_from_db_steamid(mongo_lock_proxy& ctx, uint64_t psteam_id)
{
    steam_id = psteam_id;

    nlohmann::json req;
    req["steam_id"] = steam_id;

    std::vector<nlohmann::json> found = fetch_from_db(ctx, req);

    if(found.size() != 1)
        return false;

    deserialise(found[0], *this, serialise_mode::DISK);

    return true;
}

void auth::overwrite_in_db(mongo_lock_proxy& ctx)
{
    nlohmann::json req;
    req["auth_token_hex"] = auth_token_hex;

    nlohmann::json data = serialise(*this, serialise_mode::DISK);

    update_in_db_if_exact(ctx, req, data);
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
