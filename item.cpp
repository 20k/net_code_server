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

bool item::transfer_to_user(const std::string& username, int thread_id)
{
    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);

        user temp;

        if(!temp.load_from_db(user_ctx, username))
            return false;

        ///NON BLOCKING
        int max_items = temp.get_total_user_properties(thread_id)["max_items"];

        if(temp.num_items() >= max_items)
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

bool item::transfer_from_to_by_index(int index, const std::string& from, const std::string& to, int thread_id)
{
    if(from == to)
        return true;

    user u1, u2;

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);

        if(!u1.load_from_db(user_ctx, from))
            return false;

        if(!u2.load_from_db(user_ctx, to))
            return false;
    }

    std::string item_id = u1.index_to_item(index);

    if(item_id == "")
        return false;

    if(u2.num_items() >= u2.get_total_user_properties(thread_id)["max_items"])
        return false;

    u1.remove_item(item_id);
    u2.append_item(item_id);

    {
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        load_from_db(item_ctx, item_id);
        set_prop("owner", to);
        set_prop("item_id", item_id);

        ///unregister script bundle
        if(get_prop("item_type") == std::to_string(item_types::EMPTY_SCRIPT_BUNDLE))
            set_prop("registered_as", "");

        overwrite_in_db(item_ctx);
    }

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);
        u1.overwrite_user_in_db(user_ctx);
        u2.overwrite_user_in_db(user_ctx);
    }

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
    set_prop("lock_is_breached", 0);
}

void item::breach()
{
    set_prop("lock_is_breached", 1);
}

bool item::is_breached()
{
    return get_prop("lock_is_breached") == "1";
}

std::vector<item> load_items(mongo_lock_proxy& items_ctx, const std::vector<std::string>& ids)
{
    std::vector<item> ret;

    for(auto& i : ids)
    {
        item it;

        if(!it.load_from_db(items_ctx, i))
            continue;

        ret.push_back(it);
    }

    return ret;
}

item item_types::get_default_of(item_types::item_type type, const std::string& lock_name)
{
    using namespace item_types;

    item new_item = get_default(type);

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

    if(type == ON_BREACH)
    {
        new_item.set_prop("script_name", "");
        new_item.set_prop("desc", "Runs a script when your breach node is compromised");
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

item item_types::get_named_describer(const std::string& short_name, const std::string& description)
{
    item new_item = item_types::get_default_of(item_types::MISC, "");
    new_item.set_prop("rarity", 3);
    new_item.set_prop("short_name", short_name);
    new_item.set_prop("desc", description);

    return new_item;
}

void item_types::give_item_to(item& new_item, const std::string& to, int thread_id)
{
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(thread_id);
        new_item.generate_set_id(mongo_ctx);
    }

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_items_context(thread_id);
        new_item.overwrite_in_db(mongo_ctx);
    }

    new_item.transfer_to_user(to, thread_id);
}

///need a remove from user... and then maybe pull out all the lock proxies?
///implement remove from user

///then implement transfer between users with external lock contexts
