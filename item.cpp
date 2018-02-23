#include "item.hpp"
#include "user.hpp"

int32_t item::get_new_id(mongo_lock_proxy& global_props_ctx)
{
    mongo_requester request;
    request.set_prop("items_id_is_gid", 1);

    std::vector<mongo_requester> found = request.fetch_from_db(global_props_ctx);

    if(found.size() == 0)
    {
        mongo_requester to_set;
        to_set.set_prop("items_id_is_gid", 1);
        to_set.set_prop("items_id_gid", 1);

        to_set.insert_in_db(global_props_ctx);

        return 0;
    }
    else
    {
        mongo_requester& fid = found[0];

        int32_t id = fid.get_prop_as_integer("items_id_gid");

        mongo_requester to_find;
        to_find.set_prop("items_id_is_gid", 1);

        mongo_requester to_set;
        to_set.set_prop("items_id_gid", id+1);

        to_find.update_in_db_if_exact(global_props_ctx, to_set);

        return id;
    }
}

bool item::exists_in_db(mongo_lock_proxy& ctx, const std::string& item_id)
{
    mongo_requester request;
    request.set_prop("item_id", item_id);

    return request.fetch_from_db(ctx).size() > 0;
}

void item::overwrite_in_db(mongo_lock_proxy& ctx)
{
    mongo_requester request;
    request.set_prop("item_id", properties["item_id"]);

    mongo_requester to_update;
    to_update.properties = properties;

    request.update_in_db_if_exact(ctx, to_update);
}

void item::create_in_db(mongo_lock_proxy& ctx)
{
    if(exists_in_db(ctx, properties["item_id"]))
        return;

    mongo_requester to_store;
    to_store.properties = properties;

    to_store.insert_in_db(ctx);

    std::cout << "CREATE ITEM " << std::endl;
}

void item::load_from_db(mongo_lock_proxy& ctx, const std::string& item_id)
{
    mongo_requester request;
    request.set_prop("item_id", item_id);

    auto found = request.fetch_from_db(ctx);

    for(auto& i : found)
    {
        properties = i.properties;
    }
}

bool item::transfer_to_user(const std::string& username, int thread_id)
{
    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);
        user_ctx->change_collection(username);

        mongo_requester request;
        request.set_prop("name", username);

        std::vector<mongo_requester> found = request.fetch_from_db(user_ctx);

        if(found.size() == 0)
            return false;

        mongo_requester req = found[0];

        std::string upg_str = req.get_prop("upgr_idx");

        ///uids
        std::vector<std::string> upgrades = no_ss_split(upg_str, " ");

        ///MAX UPGRADES TODO ALARM:
        if(upgrades.size() >= 128)
        {
            return false;
        }

        std::string my_id = get_prop("item_id");

        upgrades.push_back(my_id);

        std::string accum;

        for(auto& i : upgrades)
        {
            accum += i + " ";
        }

        mongo_requester to_store_user;
        to_store_user.set_prop("name", username);

        mongo_requester to_update_user;
        to_update_user.set_prop("upgr_idx", accum);

        to_store_user.update_in_db_if_exact(user_ctx, to_update_user);
    }

    {
        set_prop("owner", username);
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        overwrite_in_db(item_ctx);
    }

    return true;
}

bool item::remove_from_user(const std::string& username, int thread_id)
{
    {
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        load_from_db(item_ctx, properties["item_id"]);
    }

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);
        user_ctx->change_collection(username);

        mongo_requester request;
        request.set_prop("name", username);

        std::vector<mongo_requester> found = request.fetch_from_db(user_ctx);

        if(found.size() == 0)
            return false;

        std::cout << "post one\n";

        mongo_requester req = found[0];

        std::string upg_str = req.get_prop("upgr_idx");

        std::cout << "upg str " << upg_str << std::endl;

        ///uids
        std::vector<std::string> upgrades = no_ss_split(upg_str, " ");

        std::string my_id = get_prop("item_id");

        std::cout << "my id " << my_id << std::endl;

        auto fupgrade = std::find(upgrades.begin(), upgrades.end(), my_id);

        if(fupgrade == upgrades.end())
            return false;

        std::cout << "post 2\n";

        upgrades.erase(fupgrade);

        std::string accum;

        for(auto& i : upgrades)
        {
            accum += i + " ";
        }

        mongo_requester to_store_user;
        to_store_user.set_prop("name", username);

        mongo_requester to_update_user;
        to_update_user.set_prop("upgr_idx", accum);

        to_store_user.update_in_db_if_exact(user_ctx, to_update_user);
    }

    {
        set_prop("owner", "");
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        overwrite_in_db(item_ctx);
    }

    return true;
}

///need a remove from user... and then maybe pull out all the lock proxies?
///implement remove from user

///then implement transfer between users with external lock contexts
