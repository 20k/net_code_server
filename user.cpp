#include "user.hpp"
#include "rng.hpp"

void user::overwrite_user_in_db(mongo_lock_proxy& ctx)
{
    ctx->change_collection(name);

    mongo_requester filter;
    filter.set_prop("name", name);

    mongo_requester to_set;
    to_set.set_prop("name", name);
    to_set.set_prop_double("cash", cash);
    to_set.set_prop_int("last_message_uid", last_message_uid);
    to_set.set_prop("upgr_idx", upgr_idx);
    to_set.set_prop("loaded_upgr_idx", loaded_upgr_idx);
    to_set.set_prop("user_port", user_port);

    filter.update_in_db_if_exists(ctx, to_set);
}

bool user::exists(mongo_lock_proxy& ctx, const std::string& name_)
{
    ctx->change_collection(name_);

    bson_t* to_find = BCON_NEW("name", "{", "$exists", BCON_BOOL(true), "}");

    std::vector<std::string> ret = ctx->find_bson(name_, to_find, nullptr);

    bson_destroy(to_find);

    return ret.size() != 0;
}

bool user::load_from_db(mongo_lock_proxy& ctx, const std::string& name_)
{
    ctx->change_collection(name_);

    if(!exists(ctx, name_))
        return false;

    valid = true;

    mongo_requester request;
    request.set_prop("name", name_);

    auto found = request.fetch_from_db(ctx);

    for(mongo_requester& req : found)
    {
        if(req.has_prop("name"))
            name = req.get_prop("name");
        if(req.has_prop("cash"))
            cash = req.get_prop_as_double("cash");
        if(req.has_prop("auth"))
            auth = req.get_prop("auth");
        if(req.has_prop("last_message_uid"))
            last_message_uid = req.get_prop_as_integer("last_message_uid");
        if(req.has_prop("upgr_idx"))
            upgr_idx = req.get_prop("upgr_idx");
        if(req.has_prop("loaded_upgr_idx"))
            loaded_upgr_idx = req.get_prop("loaded_upgr_idx");
        if(req.has_prop("user_port"))
            user_port = req.get_prop("user_port");
    }

    if(user_port == "")
    {
        user_port = generate_user_port();

        overwrite_user_in_db(ctx);
    }

    return true;
}

bool user::construct_new_user(mongo_lock_proxy& ctx, const std::string& name_, const std::string& auth, int last_message_uid_)
{
    name = name_;
    last_message_uid = last_message_uid_;

    if(!is_valid_string(name))
        return false;

    if(exists(ctx, name))
        return false;

    valid = true;

    ctx->change_collection(name);

    mongo_requester request;
    request.set_prop("name", name);
    request.set_prop_bin("auth", auth);
    request.set_prop_int("last_message_uid", last_message_uid);
    request.set_prop("upgr_idx", "");
    request.set_prop("loaded_upgr_idx", "");
    request.set_prop("user_port", generate_user_port());

    request.insert_in_db(ctx);

    return true;
}

std::map<std::string, double> user::get_properties_from_loaded_items(mongo_lock_proxy& ctx)
{
    std::map<std::string, double> ret;

    std::vector<std::string> all_items = all_loaded_items();

    for(std::string& id : all_items)
    {
        item item_id;
        item_id.load_from_db(ctx, id);

        ret["char_count"] += item_id.get_prop_as_double("char_count");
        ret["script_slots"] += item_id.get_prop_as_double("script_slots");
        ret["public_script_slots"] += item_id.get_prop_as_double("public_script_slots");
    }

    return ret;
}

std::map<std::string, double> user::get_total_user_properties(mongo_lock_proxy& ctx)
{
    std::map<std::string, double> found = get_properties_from_loaded_items(ctx);

    found["char_count"] += 500;
    found["script_slots"] += 2;
    found["public_script_slots"] += 1;

    return found;
}

bool user::has_loaded_item(const std::string& id)
{
    std::vector<std::string> items = str_to_array(loaded_upgr_idx);

    for(auto& i : items)
    {
        if(i == id)
            return true;
    }

    return false;
}

bool user::load_item(const std::string& id)
{
    if(id == "")
        return false;

    if(has_loaded_item(id))
        return false;

    std::vector<std::string> items = str_to_array(loaded_upgr_idx);

    if(items.size() >= MAX_ITEMS)
        return false;

    items.push_back(id);

    loaded_upgr_idx = array_to_str(items);

    return true;
}

void user::unload_item(const std::string& id)
{
    if(id == "")
        return;

    if(!has_loaded_item(id))
        return;

    std::vector<std::string> items = str_to_array(loaded_upgr_idx);

    auto it = std::find(items.begin(), items.end(), id);

    if(it == items.end())
        return;

    items.erase(it);

    loaded_upgr_idx = array_to_str(items);
}

std::vector<std::string> user::all_loaded_items()
{
    return str_to_array(loaded_upgr_idx);
}

std::string user::get_loaded_callable_scriptname_source(mongo_lock_proxy& ctx, const std::string& full_name)
{
    std::vector<std::string> loaded = all_loaded_items();

    for(auto& id : loaded)
    {
        item next;
        next.load_from_db(ctx, id);

        if(name + "." + next.get_prop("registered_as") == full_name)
            return next.get_prop("unparsed_source");
    }

    return "";
}

std::string user::index_to_item(int index)
{
    std::vector<std::string> items = str_to_array(upgr_idx);

    if(index < 0 || index >= (int)items.size())
        return "";

    return items[index];
}

void user::append_item(const std::string& id)
{
    std::vector<std::string> items = str_to_array(upgr_idx);

    items.push_back(id);

    upgr_idx = array_to_str(items);
}

bool user::has_item(const std::string& id)
{
    std::vector<std::string> items = str_to_array(upgr_idx);

    for(auto& i : items)
    {
        if(i == id)
            return true;
    }

    return false;
}

void user::remove_item(const std::string& id)
{
    unload_item(id);

    std::vector<std::string> items = str_to_array(upgr_idx);

    auto it = std::find(items.begin(), items.end(), id);

    if(it == items.end())
        return;

    items.erase(it);

    upgr_idx = array_to_str(items);
}

int user::num_items()
{
    return str_to_array(upgr_idx).size();
}

int user::find_num_scripts(mongo_lock_proxy& ctx)
{
    mongo_requester request;
    request.set_prop("owner", name);
    request.set_prop("is_script", 1);

    std::vector<mongo_requester> results = request.fetch_from_db(ctx);

    return results.size();
}

int user::find_num_public_scripts(mongo_lock_proxy& ctx)
{
    mongo_requester request;
    request.set_prop("owner", name);
    request.set_prop("is_script", 1);
    request.set_prop("in_public", 1);

    std::vector<mongo_requester> results = request.fetch_from_db(ctx);

    return results.size();
}
