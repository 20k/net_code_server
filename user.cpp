#include "user.hpp"
#include "rng.hpp"
#include <libncclient/nc_util.hpp>
#include <secret/node.hpp>
#include "global_caching.hpp"
#include <secret/npc_manager.hpp>

using global_user_cache = global_generic_cache<user>;

global_user_cache& get_global_user_cache()
{
    static global_user_cache cache;

    return cache;
}

void user::overwrite_user_in_db(mongo_lock_proxy& ctx)
{
    ctx.change_collection(name);

    mongo_requester filter;
    filter.set_prop("name", name);

    mongo_requester to_set;
    to_set.set_prop("name", name);
    to_set.set_prop_double("cash", cash);
    to_set.set_prop("upgr_idx", upgr_idx);
    to_set.set_prop("loaded_upgr_idx", loaded_upgr_idx);
    #ifdef USE_LOCS
    to_set.set_prop("user_port", user_port);
    #endif // USE_LOCS
    to_set.set_prop("initial_connection_setup", initial_connection_setup);
    to_set.set_prop_array("owner_list", owner_list);
    to_set.set_prop_array("call_stack", call_stack);

    for(int i=0; i < decltype(pos)::DIM; i++)
        to_set.set_prop("vector_pos" + std::to_string(i), pos.v[i]);

    //for(int i=0; i < decltype(local_pos)::DIM; i++)
    //    to_set.set_prop("vector_pos_local" + std::to_string(i), local_pos.v[i]);

    to_set.set_prop("move_queue", nlohmann::json(move_queue).dump());

    to_set.set_prop("joined_channels", joined_channels);

    filter.update_in_db_if_exact(ctx, to_set);

    global_user_cache& cache = get_global_user_cache();
    cache.overwrite_in_cache(name, *this);
}

bool user::exists(mongo_lock_proxy& ctx, const std::string& name_)
{
    global_user_cache& cache = get_global_user_cache();

    if(cache.exists_in_cache(name_))
        return true;

    ctx.change_collection(name_);

    mongo_requester req;
    req.set_prop("name", name_);

    return req.fetch_from_db(ctx).size() == 1;
}

bool user::load_from_db(mongo_lock_proxy& ctx, const std::string& name_)
{
    global_user_cache& cache = get_global_user_cache();

    if(cache.exists_in_cache(name_))
    {
        *this = cache.load_from_cache(name_);
        return true;
    }

    ctx.change_collection(name_);

    if(!exists(ctx, name_))
        return false;

    bool has_pos = false;

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
        if(req.has_prop("upgr_idx"))
            upgr_idx = req.get_prop("upgr_idx");
        if(req.has_prop("loaded_upgr_idx"))
            loaded_upgr_idx = req.get_prop("loaded_upgr_idx");
        #ifdef USE_LOCS
        if(req.has_prop("user_port"))
            user_port = req.get_prop("user_port");
        #endif // USE_LOCS
        if(req.has_prop("initial_connection_setup"))
            initial_connection_setup = req.get_prop_as_integer("initial_connection_setup");
        if(req.has_prop("owner_list"))
            owner_list = req.get_prop_as_array("owner_list");
        if(req.has_prop("call_stack"))
            call_stack = req.get_prop_as_array("call_stack");

        for(int i=0; i < decltype(pos)::DIM; i++)
        {
            if(req.has_prop("vector_pos" + std::to_string(i)))
            {
                pos.v[i] = req.get_prop_as_double("vector_pos" + std::to_string(i));
                has_pos = true;
            }
        }

        /*for(int i=0; i < decltype(local_pos)::DIM; i++)
        {
            if(req.has_prop("vector_pos_local" + std::to_string(i)))
            {
                local_pos.v[i] = req.get_prop_as_double("vector_pos_local" + std::to_string(i));
                has_local_pos = true;
            }
        }*/

        if(req.has_prop("move_queue"))
        {
            //std::cout << "hello " << req.get_prop("move_queue") << std::endl;

            try
            {
                move_queue = nlohmann::json::parse(req.get_prop("move_queue"));
            }
            catch(...)
            {
                std::cout << "caught error in move queue\n";
            }
        }

        if(req.has_prop("joined_channels"))
            joined_channels = req.get_prop("joined_channels");

        all_found_props = req;
    }

    ///the reason why this is such a clustertruck is to avoid leaking
    ///what type pos is
    decltype(pos) nv;

    for(int i=0; i < decltype(nv)::DIM; i++)
    {
        nv.v[i] = 0;
    }

    if(!has_pos || pos == nv)
    {
        pos = sample_game_structure();

        overwrite_user_in_db(ctx);
    }

    /*if(local_pos == nv)
    {
        has_local_pos = false;
    }*/

    has_local_pos = move_queue.timestamp_queue.size() > 0;

    #ifdef USE_LOCS
    if(user_port == "")
    {
        user_port = generate_user_port();

        overwrite_user_in_db(ctx);
    }
    #endif // USE_LOCS

    cache.overwrite_in_cache(name_, *this);

    return true;
}

bool user::construct_new_user(mongo_lock_proxy& ctx, const std::string& name_, const std::string& auth)
{
    ctx.change_collection(name_);

    name = name_;

    if(!is_valid_string(name))
        return false;

    if(exists(ctx, name))
        return false;

    valid = true;

    mongo_requester request;
    request.set_prop("name", name);
    request.set_prop_bin("auth", auth);
    request.set_prop("upgr_idx", "");
    request.set_prop("loaded_upgr_idx", "");
    #ifdef USE_LOCS
    request.set_prop("user_port", generate_user_port());
    #endif // USE_LOCS
    request.set_prop("is_user", 1);
    request.set_prop("initial_connection_setup", initial_connection_setup);
    request.set_prop_array("owner_list", std::vector<std::string>());
    request.set_prop_array("call_stack", std::vector<std::string>());

    pos = sample_game_structure();

    for(int i=0; i < decltype(pos)::DIM; i++)
        request.set_prop("vector_pos" + std::to_string(i), pos.v[i]);

    request.set_prop("joined_channels", "");

    request.insert_in_db(ctx);

    ///not valid to cache here
    //global_user_cache& cache = get_global_user_cache();
    //cache.overwrite_in_cache(name, *this);

    return true;
}

void user::delete_from_cache(const std::string& name_)
{
    global_user_cache& cache = get_global_user_cache();
    cache.delete_from_cache(name_);
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
        ret["network_links"] += item_id.get_prop_as_double("network_links");
    }

    return ret;
}

std::map<std::string, double> user::get_total_user_properties(int thread_id)
{
    std::map<std::string, double> found;


    {
        mongo_lock_proxy ctx = get_global_mongo_user_items_context(thread_id);

        found = get_properties_from_loaded_items(ctx);
    }

    found["char_count"] += 500;
    found["script_slots"] += 2;
    found["public_script_slots"] += 1;

    {
        found["network_links"] = get_default_network_links(-2);
    }

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

/*std::string user::get_loaded_callable_scriptname_source(mongo_lock_proxy& ctx, const std::string& full_name)
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
}*/

item user::get_loaded_callable_scriptname_item(mongo_lock_proxy& ctx, const std::string& full_name)
{
    std::vector<std::string> loaded = all_loaded_items();

    for(auto& id : loaded)
    {
        item next;
        next.load_from_db(ctx, id);

        if(name + "." + next.get_prop("registered_as") == full_name)
            return next;
    }

    return item();
}

std::vector<item> user::get_all_items(mongo_lock_proxy& ctx)
{
    std::vector<std::string> all_items = str_to_array(upgr_idx);

    std::vector<item> ret;

    for(auto& id : all_items)
    {
        item next;
        next.load_from_db(ctx, id);

        ret.push_back(next);
    }

    return ret;
}

std::string user::index_to_item(int index)
{
    std::vector<std::string> items = str_to_array(upgr_idx);

    if(index < 0 || index >= (int)items.size())
        return "";

    return items[index];
}

int user::item_to_index(const std::string& item)
{
    auto items = str_to_array(upgr_idx);

    for(int i=0; i < (int)items.size(); i++)
    {
        if(items[i] == item)
            return i;
    }

    return -1;
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

void user::clear_items(int thread_id)
{
    upgr_idx = "";
    loaded_upgr_idx = "";

    user_nodes nodes = get_nodes(name, thread_id);

    for(user_node& node : nodes.nodes)
    {
        node.attached_locks = decltype(node.attached_locks)();
    }

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(thread_id);

        nodes.overwrite_in_db(node_ctx);
    }
}

int user::num_items()
{
    return str_to_array(upgr_idx).size();
}

void user::cleanup_call_stack(int thread_id)
{
    std::vector<std::string> stk = call_stack;

    if(stk.size() == 0)
        return;

    int start_valid = (int)stk.size();
    int last_valid = (int)stk.size();

    for(int i=0; i < (int)stk.size(); i++)
    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(thread_id);

        user usr;

        if(!usr.load_from_db(ctx, stk[i]))
        {
            last_valid = i;
            break;
        }

        if(!usr.is_allowed_user(name))
        {
            last_valid = i;
            break;
        }
    }

    stk.resize(last_valid);

    if(start_valid != last_valid)
    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(thread_id);

        overwrite_user_in_db(ctx);
    }
}

std::vector<std::string> user::get_call_stack()
{
    std::vector<std::string> ret{name};

    ret.insert(ret.end(), call_stack.begin(), call_stack.end());

    return ret;
}

std::vector<std::string> user::get_allowed_users()
{
    std::vector<std::string> usrs{name};

    std::vector<std::string> found = owner_list;

    usrs.insert(usrs.end(), found.begin(), found.end());

    return usrs;
}

bool user::is_allowed_user(const std::string& usr)
{
    if(!is_npc())
        return false;

    auto valid = get_allowed_users();

    for(auto& i : valid)
    {
        if(i == usr)
            return true;
    }

    return false;
}

void user::add_allowed_user(const std::string& usr, mongo_lock_proxy& ctx)
{
    if(is_allowed_user(usr))
        return;

    ctx.change_collection(usr);

    owner_list.push_back(usr);

    mongo_requester filter;
    filter.set_prop("name", name);

    mongo_requester to_set;
    to_set.set_prop("name", name);
    to_set.set_prop_array("owner_list", owner_list);

    filter.update_in_db_if_exact(ctx, to_set);
}

void user::remove_allowed_user(const std::string& usr, mongo_lock_proxy& ctx)
{
    if(!is_allowed_user(usr))
        return;

    if(usr == name)
        return;

    ctx.change_collection(name);

    owner_list.erase(std::remove(owner_list.begin(), owner_list.end(), usr), owner_list.end());

    mongo_requester filter;
    filter.set_prop("name", name);

    mongo_requester to_set;
    to_set.set_prop("name", name);
    to_set.set_prop_array("owner_list", owner_list);

    filter.update_in_db_if_exact(ctx, to_set);
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

int user::get_default_network_links(int thread_id)
{
    if(!is_npc())
        return 4;

    int base = 4;

    npc_prop_list props;

    {
        mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(thread_id);

        if(!props.load_from_db(ctx, name))
            return 4;
    }

    if(props.has("vals") && props.has("props"))
    {
        std::vector<int> enums = props.get_as<std::vector<int>>("props");
        std::vector<float> vals = props.get_as<std::vector<float>>("vals");

        for(int i=0; i < (int)enums.size(); i++)
        {
            if(enums[i] == npc_info::MAX_CONNECT)
            {
                if(vals[i] > 0)
                    base++;
                else if(vals[i] < 0)
                    base--;
            }

            #ifdef EXTRA_WARPY
            if(enums[i] == npc_info::WARPY)
            {
                base += 4;
            }
            #endif // EXTRA_WARPY
        }
    }

    if(base < 1)
        base = 1;

    return base;
}

bool user::is_npc() const
{
    return auth == "";
}

std::string user::fetch_sector()
{
    return get_nearest_structure(pos).name;
}

extern size_t get_wall_time();

space_pos_t user::get_local_pos() const
{
    size_t current_time = get_wall_time();

    ///not necessary to actually update db
    //move_queue.cleanup_old_elements(current_time);

    return move_queue.get_position_at(current_time).position;
}

void user::set_local_pos(space_pos_t pos)
{
    timestamped_position tstamp;
    tstamp.position = pos;
    tstamp.timestamp = get_wall_time();

    move_queue.timestamp_queue.clear();

    move_queue.add_queue_element(tstamp);
    has_local_pos = true;
}

void user::add_position_target(space_pos_t pos, size_t time_when_delta)
{
    timestamped_position tstamp;

    if(move_queue.timestamp_queue.size() > 0)
    {
        size_t last_tstamp = get_wall_time();

        ///if the last timestamp we have is into the future
        ///is that timestamp
        ///otherwise use current time
        if(move_queue.timestamp_queue.back().timestamp > last_tstamp)
            last_tstamp = move_queue.timestamp_queue.back().timestamp;

        timestamped_position replicated;
        replicated.position = move_queue.timestamp_queue.back().position;
        replicated.timestamp = last_tstamp;

        move_queue.add_queue_element(replicated);

        tstamp.position = pos;
        tstamp.timestamp = time_when_delta + move_queue.timestamp_queue.back().timestamp;
    }
    else
    {
        tstamp.position = pos;
        tstamp.timestamp = time_when_delta + get_wall_time();
    }

    move_queue.add_queue_element(tstamp);
    has_local_pos = true;
}

std::vector<user> load_users(const std::vector<std::string>& names, int lock_id)
{
    std::vector<user> ret;

    mongo_lock_proxy ctx = get_global_mongo_user_info_context(lock_id);

    for(auto& i : names)
    {
        user usr;

        if(!usr.load_from_db(ctx, i))
            continue;

        ret.push_back(usr);
    }

    return ret;
}
