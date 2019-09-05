#include "user.hpp"
#include "rng.hpp"
#include <libncclient/nc_util.hpp>
#include <secret/node.hpp>
#include <secret/npc_manager.hpp>
#include "privileged_core_scripts.hpp"
#include <secret/low_level_structure.hpp>
#include "logging.hpp"
#include "command_handler.hpp"
#include "safe_thread.hpp"

void to_json(nlohmann::json& j, const user_limit& limit)
{
    j = nlohmann::json{
        {"p", limit.data},
        {"t", limit.time_at},
        };
}

void from_json(const nlohmann::json& j, user_limit& limit)
{
    limit.data = j.at("p");
    limit.time_at = j.at("t");

    if(isnan(limit.data) || isinf(limit.data))
    {
        limit.data = 1;
    }
}

///could probably use this to implement the hour long window eh
double user_limit::calculate_current_data(size_t time_ms)
{
    if(time_ms < time_at)
    {
        printf("Warning, time error in calculate current data\n");

        return data;
    }

    size_t diff_ms = time_ms - time_at;

    double fraction = diff_ms / recharge_time_ms;

    if(isnan(fraction) || isinf(fraction))
    {
        fraction = 1;
    }

    //fraction = 1;

    return clamp(mix(data, 1., fraction), 0., 1.);
}

user::user()
{
    /*for(int i=0; i < user_limit::limit_type::COUNT; i++)
    {
        user_limits[i].type = i;
    }*/
}

void user::overwrite_user_in_db(mongo_lock_proxy& ctx)
{
    ctx.change_collection(name);

    mongo_requester filter;
    filter.set_prop("name", name);

    mongo_requester to_set;
    to_set.set_prop("name", name);
    to_set.set_prop("cash", cash);
    to_set.set_prop("upgr_idx", upgr_idx);
    to_set.set_prop("loaded_upgr_idx", loaded_upgr_idx);
    #ifdef USE_LOCS
    to_set.set_prop("user_port", user_port);
    #endif // USE_LOCS
    to_set.set_prop("initial_connection_setup", initial_connection_setup);
    to_set.set_prop("owner_list", owner_list);
    to_set.set_prop("call_stack", call_stack);
    to_set.set_prop("users_i_have_access_to", users_i_have_access_to);
    to_set.set_prop("auth_hex", auth_hex);
    to_set.set_prop("hacked_progress", hacked_progress);

    for(int i=0; i < decltype(pos)::DIM; i++)
        to_set.set_prop("vector_pos" + std::to_string(i), pos.v[i]);

    //for(int i=0; i < decltype(local_pos)::DIM; i++)
    //    to_set.set_prop("vector_pos_local" + std::to_string(i), local_pos.v[i]);

    to_set.set_prop("move_queue", nlohmann::json(move_queue).dump());

    to_set.set_prop("limits", nlohmann::json(user_limits).dump());

    filter.update_one_in_db_if_exact(ctx, to_set);
}

bool user::exists(mongo_lock_proxy& ctx, const std::string& name_)
{
    ctx.change_collection(name_);

    mongo_requester req;
    req.set_prop("name", name_);

    return req.fetch_from_db(ctx).size() == 1;
}

bool user::load_from_db(mongo_lock_proxy& ctx, const std::string& name_)
{
    ctx.change_collection(name_);

    if(!exists(ctx, name_))
        return false;

    bool has_pos = false;

    mongo_requester request;
    request.set_prop("name", name_);

    auto found = request.fetch_from_db(ctx);

    for(mongo_requester& req : found)
    {
        if(req.has_prop("name"))
            name = req.get_prop("name");
        if(req.has_prop("cash"))
            cash = req.get_prop("cash");
        //if(req.has_prop("auth"))
        //    old_binary_auth = req.get_prop("auth");
        if(req.has_prop("auth_hex"))
            auth_hex = req.get_prop("auth_hex");
        if(req.has_prop("hacked_progress"))
            hacked_progress = req.get_prop("hacked_progress");
        if(req.has_prop("upgr_idx"))
            upgr_idx = (std::vector<std::string>)req.get_prop("upgr_idx");
        if(req.has_prop("loaded_upgr_idx"))
            loaded_upgr_idx = (std::vector<std::string>)req.get_prop("loaded_upgr_idx");
        #ifdef USE_LOCS
        if(req.has_prop("user_port"))
            user_port = req.get_prop("user_port");
        #endif // USE_LOCS
        if(req.has_prop("initial_connection_setup"))
            initial_connection_setup = req.get_prop_as_integer("initial_connection_setup");
        if(req.has_prop("owner_list"))
            owner_list = (std::vector<std::string>)req.get_prop_as_array("owner_list");
        if(req.has_prop("call_stack"))
            call_stack = (std::vector<std::string>)req.get_prop_as_array("call_stack");
        if(req.has_prop("users_i_have_access_to"))
            users_i_have_access_to = (std::vector<std::string>)req.get_prop_as_array("users_i_have_access_to");

        for(int i=0; i < decltype(pos)::DIM; i++)
        {
            if(req.has_prop("vector_pos" + std::to_string(i)))
            {
                pos.v[i] = req.get_prop("vector_pos" + std::to_string(i));
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
                move_queue = nlohmann::json::parse((std::string)req.get_prop("move_queue"));
            }
            catch(...)
            {
                std::cout << "caught error in move queue\n";
            }
        }

        if(req.has_prop("limits"))
        {
            try
            {
                user_limits = nlohmann::json::parse((std::string)req.get_prop("limits"));
            }
            catch(...)
            {
                //std::cout << "caught error in limits" << std::endl;
            }
        }
    }

    if(!has_pos)
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

    mongo_requester request;
    request.set_prop("name", name);
    //request.set_prop_bin("auth", auth);
    request.set_prop("auth_hex", binary_to_hex(auth));
    request.set_prop("upgr_idx", std::vector<std::string>());
    request.set_prop("loaded_upgr_idx", std::vector<std::string>());
    #ifdef USE_LOCS
    request.set_prop("user_port", generate_user_port());
    #endif // USE_LOCS
    request.set_prop("is_user", 1);
    request.set_prop("initial_connection_setup", initial_connection_setup);
    request.set_prop("owner_list", std::vector<std::string>());
    request.set_prop("call_stack", std::vector<std::string>());
    request.set_prop("users_i_have_access_to", std::vector<std::string>());

    pos = sample_game_structure();

    for(int i=0; i < decltype(pos)::DIM; i++)
        request.set_prop("vector_pos" + std::to_string(i), pos.v[i]);

    request.insert_in_db(ctx);

    return true;
}

std::string user::get_auth_token_hex()
{
    return auth_hex;
}

std::string user::get_auth_token_binary()
{
    return hex_to_binary(auth_hex);
}

void user::delete_from_cache(const std::string& name_)
{

}

std::map<std::string, double> user::get_properties_from_loaded_items(mongo_lock_proxy& ctx)
{
    std::map<std::string, double> ret;

    std::vector<std::string> all_items = all_loaded_items();

    for(std::string& id : all_items)
    {
        item item_id;
        item_id.load_from_db(ctx, id);

        ret["char_count"] += (int)item_id.get("char_count");
        ret["script_slots"] += (int)item_id.get("script_slots");
        ret["public_script_slots"] += (int)item_id.get("public_script_slots");
        ret["network_links"] += (int)item_id.get("network_links");
    }

    return ret;
}

std::map<std::string, double> user::get_total_user_properties(int thread_id)
{
    std::map<std::string, double> found;


    {
        mongo_nolock_proxy ctx = get_global_mongo_user_items_context(thread_id);

        found = get_properties_from_loaded_items(ctx);
    }

    found["char_count"] += 500;
    found["script_slots"] += 2;
    found["public_script_slots"] += 1;

    {
        found["network_links"] = get_default_network_links(-2);
    }

    found["max_items"] = MAX_ITEMS;

    if(is_npc())
    {
        npc_prop_list props;

        bool valid = false;

        {
            mongo_nolock_proxy ctx = get_global_mongo_npc_properties_context(thread_id);

            valid = props.load_from_db(ctx, name);
        }

        if(valid && props.has("vals") && props.has("props"))
        {
            std::vector<int> enums = props.get_as<std::vector<int>>("props");
            std::vector<float> vals = props.get_as<std::vector<float>>("vals");

            for(int i=0; i < (int)enums.size(); i++)
            {
                if(enums[i] == npc_info::ITEM_CAP)
                {
                    if(vals[i] > 0)
                        found["max_items"] = MAX_ITEMS * 4;
                    else if(vals[i] < 0)
                        found["max_items"] = MAX_ITEMS / 2;
                }
            }
        }
    }

    return found;
}

bool user::has_loaded_item(const std::string& id)
{
    std::vector<std::string> items = loaded_upgr_idx;

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

    std::vector<std::string> items = loaded_upgr_idx;

    ///non blocking get total user properties
    if(items.size() >= get_total_user_properties(-2)["max_items"])
        return false;

    items.push_back(id);

    loaded_upgr_idx = items;

    return true;
}

void user::unload_item(const std::string& id)
{
    if(id == "")
        return;

    if(!has_loaded_item(id))
        return;

    std::vector<std::string> items = loaded_upgr_idx;

    auto it = std::find(items.begin(), items.end(), id);

    if(it == items.end())
        return;

    items.erase(it);

    loaded_upgr_idx = items;
}

std::vector<std::string> user::all_loaded_items()
{
    return loaded_upgr_idx;
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
    std::vector<std::string> all_items = upgr_idx;

    std::vector<item> ret;

    for(auto& id : all_items)
    {
        item next;

        if(!next.load_from_db(ctx, id))
            continue;

        ret.push_back(next);
    }

    return ret;
}

std::vector<std::string> user::get_all_items()
{
    return upgr_idx;
}

std::string user::index_to_item(int index)
{
    std::vector<std::string> items = upgr_idx;

    if(index < 0 || index >= (int)items.size())
        return "";

    return items[index];
}

int user::item_to_index(const std::string& item)
{
    auto items = upgr_idx;

    for(int i=0; i < (int)items.size(); i++)
    {
        if(items[i] == item)
            return i;
    }

    return -1;
}

void user::append_item(const std::string& id)
{
    std::vector<std::string> items = upgr_idx;

    items.push_back(id);

    upgr_idx = items;
}

bool user::has_item(const std::string& id)
{
    std::vector<std::string> items = upgr_idx;

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

    std::vector<std::string> items = upgr_idx;

    auto it = std::find(items.begin(), items.end(), id);

    if(it == items.end())
        return;

    items.erase(it);

    upgr_idx = items;
}

void user::clear_items(int thread_id)
{
    upgr_idx = {};
    loaded_upgr_idx = {};

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
    return upgr_idx.size();
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
        mongo_nolock_proxy ctx = get_global_mongo_user_info_context(thread_id);

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
        mongo_nolock_proxy ctx = get_global_mongo_user_info_context(thread_id);

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

std::vector<std::string> user::get_users_i_have_access_to()
{
    return users_i_have_access_to;
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
    to_set.set_prop("owner_list", owner_list);

    filter.update_in_db_if_exact(ctx, to_set);

    user found_user;

    if(found_user.load_from_db(ctx, usr))
    {
        found_user.users_i_have_access_to.push_back(name);

        found_user.overwrite_user_in_db(ctx);
    }
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
    to_set.set_prop("owner_list", owner_list);

    filter.update_in_db_if_exact(ctx, to_set);

    user found_user;

    if(found_user.load_from_db(ctx, usr))
    {
        ///find my name in their access list
        //auto found_it = found_user.users_i_have_access_to.find(name);

        auto found_it = std::find(found_user.users_i_have_access_to.begin(), found_user.users_i_have_access_to.end(), name);

        ///if not the end iterator, erase my name from their access list
        if(found_it != found_user.users_i_have_access_to.end())
        {
            found_user.users_i_have_access_to.erase(found_it);
            found_user.overwrite_user_in_db(ctx);
        }
    }
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
        mongo_nolock_proxy ctx = get_global_mongo_npc_properties_context(thread_id);

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
    return auth_hex == "";
}

std::string user::fetch_sector()
{
    return get_nearest_structure(pos).name;
}

double user::get_pvp_old_cash_estimate(size_t current_time)
{
    user_limit& lim = user_limits[user_limit::CASH_STEAL];

    double current_frac = lim.calculate_current_data(current_time);

    if(current_frac < 0.01)
        return 0.;

    current_frac = clamp(current_frac, 0.01, 1.);

    return cash / current_frac;
}

double user::get_max_stealable_cash(size_t current_time, low_level_structure& sys)
{
    double old_cash = get_pvp_old_cash_estimate(current_time);

    if(isnan(old_cash))
    {
        printf("isnan old cashn");
    }

    double system_ratelimit_max_cash_steal_percentage = sys.get_ratelimit_max_cash_percentage_steal();
    double stealable_cash_constant = old_cash * system_ratelimit_max_cash_steal_percentage;

    if(isnan(system_ratelimit_max_cash_steal_percentage))
    {
        printf("isnan system ratelimit cash\n");
    }

    if(isnan(stealable_cash_constant))
    {
        printf("isnan stealable cash constant\n");
    }

    user_limit& lim = user_limits[user_limit::CASH_STEAL];

    double real_cash_steal_limit = stealable_cash_constant - (1.f - lim.calculate_current_data(current_time)) * old_cash;

    if(isnan(real_cash_steal_limit))
    {
        printf("isnan real cash steal limit\n");
    }

    return real_cash_steal_limit;
}

double user::get_max_sendable_cash(size_t current_time, low_level_structure& sys_1, low_level_structure& sys_2)
{
    double system_ratelimit_max_cash_send = get_most_secure_seclevel_of(sys_1, sys_2).get_ratelimit_max_cash_send();

    user_limit& lim = user_limits[user_limit::CASH_SEND];

    double real_cash_limit = system_ratelimit_max_cash_send * lim.calculate_current_data(current_time);

    return real_cash_limit;
}

double user::get_max_stealable_items(size_t current_time, low_level_structure& sys)
{
    double system_ratelimit_max_item_steal = sys.get_ratelimit_max_item_steal();

    user_limit& lim = user_limits[user_limit::ITEM_STEAL];

    double real_item_limit = floor(system_ratelimit_max_item_steal * lim.calculate_current_data(current_time));

    return real_item_limit;
}

double user::get_max_sendable_items(size_t current_time, low_level_structure& sys_1, low_level_structure& sys_2)
{
    double system_ratelimit_max_item_send = get_most_secure_seclevel_of(sys_1, sys_2).get_ratelimit_max_item_send();

    user_limit& lim = user_limits[user_limit::ITEM_SEND];

    double real_item_limit = floor(system_ratelimit_max_item_send * lim.calculate_current_data(current_time));

    return real_item_limit;
}

void user::deplete_max_stealable_items(double amount, size_t current_time, low_level_structure& sys)
{
    double real_stealable = get_max_stealable_items(current_time, sys);

    if(amount > real_stealable)
        return;

    if(fabs(real_stealable) < 0.001)
        return;

    double fraction_removed = amount / sys.get_ratelimit_max_item_steal();

    user_limit& lim = user_limits[user_limit::ITEM_STEAL];

    lim.data = clamp(lim.calculate_current_data(current_time) - fraction_removed, 0., 1.);
    lim.time_at = current_time;
}

void user::deplete_max_sendable_items(double amount, size_t current_time, low_level_structure& sys_1, low_level_structure& sys_2)
{
    if(&sys_1 == &sys_2)
        return;

    double real_sendable = get_max_sendable_items(current_time, sys_1, sys_2);

    if(amount > real_sendable)
        return;

    if(fabs(real_sendable) < 0.001)
        return;

    double fraction_removed = amount / get_most_secure_seclevel_of(sys_1, sys_2).get_ratelimit_max_item_send();

    user_limit& lim = user_limits[user_limit::ITEM_SEND];

    lim.data = clamp(lim.calculate_current_data(current_time) - fraction_removed, 0., 1.);
    lim.time_at = current_time;
}

///later loop through items and calculate it depending on defensive upgrades
///0 -> n
double user::calculate_hack_hardness()
{
    return 0;
}

double user::calculate_attack_hack_speedup()
{
    return 0;
}

double user::calculate_hack_progress()
{
    return hacked_progress;
}

extern size_t get_wall_time();

timestamp_move_queue user::get_timestamp_queue()
{
    return move_queue;
}

space_pos_t user::get_local_pos() const
{
    size_t current_time = get_wall_time();

    ///not necessary to actually update db
    //move_queue.cleanup_old_elements(current_time);

    return move_queue.get_position_at(current_time).position;
}

timestamped_position user::get_final_pos() const
{
    timestamped_position pos;

    for(int i=0; i < (int)move_queue.timestamp_queue.size(); i++)
    {
        const timestamped_position& q = move_queue.timestamp_queue[i];

        if(q.is_blocking())
            continue;

        pos = q;
    }

    return pos;
}

void user::set_local_pos(space_pos_t pos, int replace_item_at)
{
    timestamped_position tstamp;
    tstamp.position = pos;
    tstamp.timestamp = get_wall_time();

    if(replace_item_at == -1)
    {
        move_queue.timestamp_queue.clear();

        move_queue.add_queue_element(tstamp);
    }
    else
    {
        if(replace_item_at >= 0 && replace_item_at < (int)move_queue.timestamp_queue.size())
        {
            move_queue.timestamp_queue[replace_item_at] = tstamp;
        }
        else
        {
            std::cout << "set local pos replace item warning at " << replace_item_at << " with user " << name << std::endl;
        }
    }

    has_local_pos = true;
}

void user::add_position_target(space_pos_t pos, size_t time_when_delta, std::string notif_on_finish)
{
    timestamped_position tstamp;
    tstamp.notif_on_finish = notif_on_finish;

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

void user::add_activate_target(size_t current_time, const std::string& destination_sys)
{
    move_queue.add_activate_element(current_time, destination_sys);
}

void user::reset_internal_queue()
{
    set_local_pos(get_local_pos());
}

void user::pump_notifications(int lock_id)
{
    if(move_queue.timestamp_queue.size() == 0)
        return;

    bool any_pumped = false;

    size_t current_time = get_wall_time();

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    for(int i=0; i < (int)move_queue.timestamp_queue.size(); i++)
    {
        timestamped_position& q = move_queue.timestamp_queue[i];

        ///handle any notifs
        if(current_time >= q.timestamp && q.notif_on_finish != "")
        {
            create_notification(lock_id, name, q.notif_on_finish);

            q.notif_on_finish = "";

            any_pumped = true;
        }

        ///handle any activation requests
        if(current_time >= q.timestamp && q.is_activate())
        {
            //bool needs_erase = false;

            #ifdef DEBUG_WARP
            std::cout <<" hi there! " << name << std::endl;
            #endif // DEBUG_WARP

            std::optional<low_level_structure*> current_sys_opt = low_level_structure_manage.get_system_of(name);
            std::optional<low_level_structure*> target_sys_opt = low_level_structure_manage.get_system_from_name(q.system_to_arrive_at);

            if(current_sys_opt.has_value() && target_sys_opt.has_value())
            {
                #ifdef DEBUG_WARP
                std::cout << "happy-1" << std::endl;
                #endif // DEBUG_WARP

                low_level_structure& current_system = *current_sys_opt.value();
                low_level_structure& target_system = *target_sys_opt.value();

                auto opt_users = low_level_structure_manage.get_connecting_npcs(current_system, target_system);

                if(opt_users.has_value())
                {
                    #ifdef DEBUG_WARP
                    std::cout << "happy-2" << std::endl;
                    #endif // DEBUG_WARP

                    user& u1 = opt_users.value().first;
                    user& u2 = opt_users.value().second;

                    #ifdef DEBUG_WARP
                    std::cout << "attempted to warp from " << u1.name << " to " << u2.name << std::endl;
                    #endif // DEBUG_WARP

                    float my_dist = (get_local_pos() - u1.get_local_pos()).length();

                    if(my_dist <= MAXIMUM_WARP_DISTANCE * 1.2f)
                    {
                        #ifdef DEBUG_WARP
                        std::cout << "happy-3" << std::endl;
                        #endif // DEBUG_WARP

                        playspace_network_manage.unlink_all(name);

                        target_system.steal_user(*this, current_system, u2.get_local_pos(), u1.get_local_pos(), i);

                        create_notification(lock_id, name, make_notif_col("-Arrived at " + *target_system.name + "-"));

                        #ifdef DEBUG_WARP
                        ///the problem is that we reset the move queue in steal user
                        ///which alters the queue
                        std::cout << "set pos " << get_local_pos() << std::endl;
                        #endif // DEBUG_WARP
                    }
                    else
                    {
                        create_notification(lock_id, name, make_notif_col("-Could not activate, out of range-"));

                        reset_internal_queue();

                        #ifdef DEBUG_WARP
                        std::cout << "max warp distance exceeded " << my_dist << std::endl;
                        std::cout << "attempted to warp from " << u1.name << " to " << u2.name << std::endl;
                        #endif // DEBUG_WARP
                    }
                }
                else
                {
                    create_notification(lock_id, name, make_notif_col("-Could not establish link between systems (internal server error?)-"));

                    reset_internal_queue();

                    lg::log("no opt user");
                }
            }
            else
            {
                reset_internal_queue();

                lg::log("error, you are lost ", name);
            }

            any_pumped = true;

            ///no need to erase here, as we update this element to be a new item type
            /*move_queue.timestamp_queue.erase(move_queue.timestamp_queue.begin() + i);
            i--;
            continue;*/
        }
    }

    if(any_pumped)
    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(lock_id);

        overwrite_user_in_db(mongo_ctx);
    }
}

void event_pumper()
{
    while(1)
    {
        for_each_user([](user& usr)
                      {
                        usr.pump_notifications(-2);
                        sthread::this_sleep(1);
                      });

        for_each_npc([](user& usr)
                     {
                        usr.pump_notifications(-2);
                        sthread::this_sleep(1);
                     });

        sthread::this_sleep(1);
    }
}

void user::launch_pump_events_thread()
{
    sthread(event_pumper).detach();
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

std::vector<user> load_users_nolock(const std::vector<std::string>& names)
{
    std::vector<user> ret;

    mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);

    for(auto& i : names)
    {
        user usr;

        if(!usr.load_from_db(ctx, i))
            continue;

        ret.push_back(usr);
    }

    return ret;
}
