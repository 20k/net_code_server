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

std::vector<std::string> str_to_array(const std::string& str)
{
    return no_ss_split(str, " ");
}

std::string array_to_str(const std::vector<std::string>& arr)
{
    std::string accum;

    for(auto& i : arr)
    {
        accum += i + " ";
    }

    return accum;
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
        std::vector<std::string> upgrades = str_to_array(upg_str);

        ///MAX UPGRADES TODO ALARM:
        if(upgrades.size() >= 128)
        {
            return false;
        }

        std::string my_id = get_prop("item_id");

        upgrades.push_back(my_id);

        std::string accum = array_to_str(upgrades);

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

        mongo_requester req = found[0];

        std::string upg_str = req.get_prop("upgr_idx");

        ///uids
        std::vector<std::string> upgrades = str_to_array(upg_str);

        std::string my_id = get_prop("item_id");

        auto fupgrade = std::find(upgrades.begin(), upgrades.end(), my_id);

        if(fupgrade == upgrades.end())
            return false;

        upgrades.erase(fupgrade);

        std::string accum = array_to_str(upgrades);

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

///this needs to take an item index in the future
bool item::transfer_from_to(const std::string& from, const std::string& to, int thread_id)
{
    ///we need to keep the item context locked to ensure that nothing else modifies the item while we're gone
    ///this involves double locking so be careful
    mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

    load_from_db(item_ctx, properties["item_id"]);

    mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);

    mongo_requester to_r;
    to_r.set_prop("name", to);

    mongo_requester from_r;
    from_r.set_prop("name", from);

    user_ctx->change_collection(to);
    auto to_found = to_r.fetch_from_db(user_ctx);

    user_ctx->change_collection(from);
    auto from_found = from_r.fetch_from_db(user_ctx);

    std::cout << "prefound\n";

    if(to_found.size() == 0 || from_found.size() == 0)
        return false;

    mongo_requester to_req = to_found[0];
    mongo_requester from_req = from_found[0];

    std::vector<std::string> to_upgrades = str_to_array(to_req.get_prop("upgr_idx"));
    std::vector<std::string> from_upgrades = str_to_array(from_req.get_prop("upgr_idx"));

    std::string item_id = properties["item_id"];

    std::cout << "preitem\n";

    if(item_id == "")
        return false;

    std::cout << "upgraedeid\n";

    std::cout << "found id of " << item_id << std::endl;

    for(auto& i : from_upgrades)
    {
        std::cout << "i " << i << std::endl;
    }

    auto upgrade_it = std::find(from_upgrades.begin(), from_upgrades.end(), item_id);

    if(upgrade_it == from_upgrades.end())
        return false;

    std::cout << "toupgr\n";

    int MAX_STORED_UPGRADES = 128;

    if((int)to_upgrades.size() >= MAX_STORED_UPGRADES)
        return false;

    std::cout << "reerer\n";

    from_upgrades.erase(upgrade_it);
    to_upgrades.push_back(item_id);

    set_prop("owner", to);

    mongo_requester to_select;
    to_select.set_prop("name", to);

    mongo_requester from_select;
    from_select.set_prop("name", from);

    mongo_requester to_update;
    to_update.set_prop("upgr_idx", array_to_str(to_upgrades));

    mongo_requester from_update;
    from_update.set_prop("upgr_idx", array_to_str(from_upgrades));

    user_ctx->change_collection(to);
    to_select.update_in_db_if_exact(user_ctx, to_update);

    user_ctx->change_collection(from);
    from_select.update_in_db_if_exact(user_ctx, from_update);

    overwrite_in_db(item_ctx);

    return true;
}

///need a remove from user... and then maybe pull out all the lock proxies?
///implement remove from user

///then implement transfer between users with external lock contexts
