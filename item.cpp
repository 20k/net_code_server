#include "item.hpp"
#include "user.hpp"
#include <libncclient/nc_util.hpp>
#include "rng.hpp"

int32_t item::get_new_id(db::read_write_tx& tx)
{
    return db::get_next_id(tx);
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

bool item::transfer_to_user(const std::string& username, int thread_id)
{
    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(thread_id);

        user temp;

        if(!temp.load_from_db(user_ctx, username))
            return false;

        ///NON BLOCKING
        int max_items = temp.get_total_user_properties()["max_items"];

        if(temp.num_items() >= max_items)
            return false;

        std::string my_id = item_id;

        temp.append_item(my_id);

        temp.overwrite_user_in_db(user_ctx);
    }

    {
        set_as("owner", username);
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        db_disk_overwrite(item_ctx, *this);
    }

    return true;
}

bool item::transfer_from_to_by_index(int index, user& u1, user& u2, db::read_write_tx& rwtx)
{
    if(u1.name == u2.name)
        return true;

    ///u1 is from
    ///u2 is to

    std::string item_id = u1.index_to_item(index);

    if(item_id == "")
        return false;

    if(u2.num_items() >= u2.get_total_user_properties()["max_items"])
        return false;

    u1.remove_item(item_id);
    u2.append_item(item_id);

    db_disk_load(rwtx, *this, item_id);

    set_as("owner", u2.name);
    set_as("item_id", item_id);

    ///unregister script bundle
    if((int)get_int("item_type") == item_types::EMPTY_SCRIPT_BUNDLE)
        set_as("registered_as", "");

    db_disk_overwrite(rwtx, *this);

    return true;
}

bool item::should_rotate()
{
    if((int)get_int("item_type") != (int)item_types::LOCK)
        return false;

    double time_s = get_wall_time_s();
    double internal_time_s = get_double("lock_last_rotate_s");

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
    set_as("lock_last_rotate_s", get_wall_time_s());
    set_as("lock_state", get_random_uint32_t());
    set_as("lock_is_breached", 0);
}

void item::breach()
{
    set_as("lock_is_breached", 1);
}

bool item::is_breached()
{
    return get_int("lock_is_breached") == 1;
}

void fix_int(nlohmann::json& val, const std::string& str)
{
    if(val.count(str) == 0)
        return;

    try
    {
        (int)val[str];
    }
    catch(...)
    {
        val[str] = 0;
    }
}

void item::fix()
{
    fix_int(data, "char_count");
    fix_int(data, "script_slots");
    fix_int(data, "public_script_slots");
    fix_int(data, "max_script_size");
    fix_int(data, "open_source");
    fix_int(data, "full");
    fix_int(data, "misc");
    fix_int(data, "lock_is_breached");
    fix_int(data, "network_links");
}

std::vector<item> load_items(mongo_lock_proxy& items_ctx, const std::vector<std::string>& ids)
{
    std::vector<item> ret;

    for(auto& i : ids)
    {
        item it;

        if(!db_disk_load(items_ctx, it, i))
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
        new_item.set_as("short_name", quick_names[(int)type]);

    if(type == LOCK)
    {
        new_item.set_as("lock_type", lock_name);
        new_item.set_as("short_name", lock_name);
        new_item.set_as("lock_state", get_random_uint32_t());
        new_item.set_as("lock_last_rotate_s", get_wall_time_s());
    }

    if(type == CHAR_COUNT)
    {
        new_item.set_as("char_count", 500);
        new_item.set_as("desc", "Increases the max number of chars you can have in a script");
    }

    if(type == SCRIPT_SLOT)
    {
        new_item.set_as("script_slots", 1);
        new_item.set_as("desc", "Increases the number of scripts you can have uploaded");
    }

    if(type == PUBLIC_SCRIPT_SLOT)
    {
        new_item.set_as("public_script_slots", 1);
        new_item.set_as("desc", "Increases the number of public scripts you can have uploaded");
    }

    if(type == EMPTY_SCRIPT_BUNDLE)
    {
        new_item.set_as("max_script_size", 500);
        new_item.set_as("open_source", 0);
        new_item.set_as("desc", "Container for a tradeable script");
        new_item.set_as("full", 0);
        new_item.set_as("registered_as", "");
        new_item.set_as("in_public", 0);
    }

    if(type == MISC)
    {
        new_item.set_as("misc", 1);
        new_item.set_as("desc", "???");
    }

    if(type == AUTO_SCRIPT_RUNNER)
    {
        new_item.set_as("run_every_s", 60*10);
        new_item.set_as("last_run", 0);
    }

    if(type == ON_BREACH)
    {
        new_item.set_as("script_name", "");
        new_item.set_as("desc", "Runs a script when your breach node is compromised");
    }

    return new_item;
}

item item_types::get_default(item_types::item_type type)
{
    item new_item;
    new_item.set_as("item_type", (int)type);
    new_item.set_as("rarity", 0);
    new_item.set_as("native_item", 1); ///identifies this class of item, separates it from built in scripts
    new_item.set_as("tier", 0);

    return new_item;
}

item item_types::get_named_describer(const std::string& short_name, const std::string& description)
{
    item new_item = item_types::get_default_of(item_types::MISC, "");
    new_item.set_as("rarity", 3);
    new_item.set_as("short_name", short_name);
    new_item.set_as("desc", description);

    return new_item;
}

void item_types::give_item_to(item& new_item, const std::string& to, int thread_id)
{
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(thread_id);

        new_item.generate_set_id(mongo_ctx);

        db_disk_overwrite(mongo_ctx, new_item);
    }

    new_item.transfer_to_user(to, thread_id);
}

///need a remove from user... and then maybe pull out all the lock proxies?
///implement remove from user

///then implement transfer between users with external lock contexts
