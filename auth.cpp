#include "auth.hpp"
#include "mongo.hpp"
#include <libncclient/nc_util.hpp>
#include "command_handler.hpp"
#include <networking/serialisable.hpp>
#include "serialisables.hpp"
#include "command_handler_fiber_backend.hpp"

bool auth::load_from_db(db::read_tx& ctx, const std::string& auth_binary_in)
{
    std::string auth_found = binary_to_hex(auth_binary_in);

    return db_disk_load(ctx, *this, auth_found);
}

///THIS IS REALLY BAD
bool auth::load_from_db_steamid(db::read_tx& ctx, uint64_t psteam_id)
{
    std::vector<auth> all_auth = db_disk_load_all(ctx, auth());

    for(auto& i : all_auth)
    {
        if(i.steam_id == psteam_id)
        {
            *this = i;
            return true;
        }
    }

    return false;
}

void auth::overwrite_in_db(db::read_write_tx& ctx)
{
    db_disk_overwrite(ctx, *this);
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

bool auth::contains_user(const std::string& username)
{
    for(const auto& i : users)
    {
        if(i == username)
            return true;
    }

    return false;
}

enforce_constant_time::~enforce_constant_time()
{
    float diff = clk.get_elapsed_time_s() * 1000;

    if(diff >= 100)
        return;

    fiber_sleep(100 - diff);
}
