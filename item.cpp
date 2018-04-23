#include "item.hpp"
#include "user.hpp"
#include <libncclient/nc_util.hpp>
#include "rng.hpp"

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
    request.set_prop("item_id", props.get_prop("item_id"));

    mongo_requester to_update = props;

    request.update_in_db_if_exact(ctx, to_update);
}

void item::create_in_db(mongo_lock_proxy& ctx)
{
    if(exists_in_db(ctx, props.get_prop("item_id")))
        return;

    mongo_requester to_store = props;

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
        props = i;
    }
}

bool array_contains(const std::vector<std::string>& arr, const std::string& str)
{
    for(auto& i : arr)
    {
        if(i == str)
            return true;
    }

    return false;
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

#define MAX_ITEMS 128

bool item::transfer_to_user(const std::string& username, int thread_id)
{
    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);

        user temp;
        temp.load_from_db(user_ctx, username);

        if(!temp.valid)
            return false;

        if(temp.num_items() >= MAX_ITEMS)
            return false;

        std::string my_id = get_prop("item_id");

        temp.append_item(my_id);

        temp.overwrite_user_in_db(user_ctx);
    }

    {
        set_prop("owner", username);
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        overwrite_in_db(item_ctx);
    }

    return true;
}

#if 0
bool item::remove_from_user(const std::string& username, int thread_id)
{
    {
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        load_from_db(item_ctx, properties["item_id"]);
    }

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);

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

    auto to_found = to_r.fetch_from_db(user_ctx);

    auto from_found = from_r.fetch_from_db(user_ctx);

    if(to_found.size() == 0 || from_found.size() == 0)
        return false;

    mongo_requester to_req = to_found[0];
    mongo_requester from_req = from_found[0];

    std::vector<std::string> to_upgrades = str_to_array(to_req.get_prop("upgr_idx"));
    std::vector<std::string> from_upgrades = str_to_array(from_req.get_prop("upgr_idx"));

    std::string item_id = properties["item_id"];

    if(item_id == "")
        return false;

    auto upgrade_it = std::find(from_upgrades.begin(), from_upgrades.end(), item_id);

    if(upgrade_it == from_upgrades.end())
        return false;

    int MAX_STORED_UPGRADES = 128;

    if((int)to_upgrades.size() >= MAX_STORED_UPGRADES)
        return false;

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

    user_ctx.change_collection(to);
    to_select.update_in_db_if_exact(user_ctx, to_update);

    user_ctx.change_collection(from);
    from_select.update_in_db_if_exact(user_ctx, from_update);

    overwrite_in_db(item_ctx);

    return true;
}
#endif // 0

bool item::transfer_from_to_by_index(int index, const std::string& from, const std::string& to, int thread_id)
{
    if(from == to)
        return true;

    mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);

    user u1, u2;
    u1.load_from_db(user_ctx, from);
    u2.load_from_db(user_ctx, to);

    if(!u1.valid || !u2.valid)
        return false;

    std::string item_id = u1.index_to_item(index);

    if(item_id == "")
        return false;

    if(u2.num_items() >= MAX_ITEMS)
        return false;

    u1.remove_item(item_id);
    u2.append_item(item_id);

    mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

    load_from_db(item_ctx, item_id);
    set_prop("owner", to);
    set_prop("item_id", item_id);

    ///unregister script bundle
    if(get_prop("item_type") == std::to_string(item_types::EMPTY_SCRIPT_BUNDLE))
        set_prop("registered_as", "");

    overwrite_in_db(item_ctx);

    u1.overwrite_user_in_db(user_ctx);
    u2.overwrite_user_in_db(user_ctx);

    return true;
}

bool item::should_rotate()
{
    if(get_prop_as_integer("item_type") != (int)item_types::LOCK)
        return false;

    double time_s = get_wall_time_s();
    double internal_time_s = get_prop_as_double("lock_last_rotate_s");

    if(time_s >= internal_time_s + item_types::rotation_time_s)
        return true;

    return false;
}

void item::handle_rotate()
{
    if(!should_rotate())
        return;

    force_rotate();
}

void item::force_rotate()
{
    set_prop("lock_last_rotate_s", get_wall_time_s());
    set_prop("lock_state", get_random_uint32_t());
}

item item_types::get_default_of(item_types::item_type type, const std::string& lock_name)
{
    using namespace item_types;

    item new_item = get_default(item_types::LOCK);

    if(type < quick_names.size() && type >= 0)
        new_item.set_prop("short_name", quick_names[(int)type]);

    if(type == LOCK)
    {
        new_item.set_prop("lock_type", lock_name);
        new_item.set_prop("short_name", lock_name);
        new_item.set_prop("lock_state", get_random_uint32_t());
        new_item.set_prop("lock_last_rotate_s", get_wall_time_s());
    }

    if(type == CHAR_COUNT)
    {
        new_item.set_prop("char_count", 500);
        new_item.set_prop("desc", "Increases the max number of chars you can have in a script");
    }

    if(type == SCRIPT_SLOT)
    {
        new_item.set_prop("script_slots", 1);
        new_item.set_prop("desc", "Increases the number of scripts you can have uploaded");
    }

    if(type == PUBLIC_SCRIPT_SLOT)
    {
        new_item.set_prop("public_script_slots", 1);
        new_item.set_prop("desc", "Increases the number of public scripts you can have uploaded");
    }

    if(type == EMPTY_SCRIPT_BUNDLE)
    {
        new_item.set_prop("max_script_size", 500);
        new_item.set_prop("open_source", 0);
        new_item.set_prop("desc", "Container for a tradeable script");
        new_item.set_prop("full", 0);
        new_item.set_prop("registered_as", "");
        new_item.set_prop("in_public", "0");
    }

    if(type == MISC)
    {
        new_item.set_prop("misc", 1);
        new_item.set_prop("desc", "???");
    }

    if(type == AUTO_SCRIPT_RUNNER)
    {
        new_item.set_prop("run_every_s", 60*10);
        new_item.set_prop("last_run", 0);
    }

    return new_item;
}

item item_types::get_default(item_types::item_type type)
{
    item new_item;
    new_item.set_prop("item_type", (int)type);
    new_item.set_prop("rarity", 0);
    new_item.set_prop("native_item", 1); ///identifies this class of item, separates it from built in scripts
    new_item.set_prop("tier", "0");

    return new_item;
}

void item_types::give_item_to(item& new_item, const std::string& to, int thread_id)
{
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(thread_id);
        new_item.generate_set_id(mongo_ctx);
    }

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(thread_id);
        new_item.create_in_db(mongo_ctx);
    }

    new_item.transfer_to_user(to, thread_id);
}

///need a remove from user... and then maybe pull out all the lock proxies?
///implement remove from user

///then implement transfer between users with external lock contexts
