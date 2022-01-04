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
#include <networking/serialisable.hpp>
#include "serialisables.hpp"
#include "command_handler_fiber_backend.hpp"

template<typename T, typename U>
void erase_remove(T& v, const U& val)
{
    v.erase(std::remove(v.begin(), v.end(), val), v.end());
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

}

void user::overwrite_user_in_db(db::read_write_tx& ctx)
{
    //ctx.change_collection(name);

    db_disk_overwrite(ctx, *this);
}

bool user::exists(db::read_tx& ctx, const std::string& name_)
{
    //ctx.change_collection(name_);
    name = name_;

    return db_disk_exists(ctx, *this);
}

bool user::load_from_db(db::read_tx& ctx, const std::string& name_)
{
    //ctx.change_collection(name_);

    try
    {
        return db_disk_load(ctx, *this, name_);
    }
    catch(...)
    {
        std::cout << "Invalid user " << name_ << std::endl;

        return false;
    }
}

bool user::construct_new_user(db::read_write_tx& ctx, const std::string& name_, const std::string& auth)
{
    //ctx.change_collection(name_);

    name = name_;
    auth_hex = binary_to_hex(auth);

    if(!is_valid_string(name))
        return false;

    if(exists(ctx, name))
        return false;

    db_disk_overwrite(ctx, *this);
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

std::map<std::string, double> user::get_properties_from_loaded_items(db::read_tx& ctx)
{
    std::map<std::string, double> ret;

    std::vector<std::string> all_items = all_loaded_items();

    for(std::string& id : all_items)
    {
        item item_id;
        db_disk_load(ctx, item_id, id);

        ret["char_count"] += (int)item_id.get_int("char_count");
        ret["script_slots"] += (int)item_id.get_int("script_slots");
        ret["public_script_slots"] += (int)item_id.get_int("public_script_slots");
        ret["network_links"] += (int)item_id.get_int("network_links");
    }

    return ret;
}

std::map<std::string, double> user::get_total_user_properties()
{
    std::map<std::string, double> found;

    db::read_tx ctx;

    found = get_properties_from_loaded_items(ctx);

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

        bool valid = db_disk_load(ctx, props, name);

        if(valid)
        {
            std::optional<float> val = props.get_prop(npc_info::ITEM_CAP);

            if(val.has_value())
            {
                if(val.value() > 0)
                    found["max_items"] = MAX_ITEMS * 4;
                else if(val.value() < 0)
                    found["max_items"] = MAX_ITEMS / 2;
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
    if(items.size() >= get_total_user_properties()["max_items"])
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

    erase_remove(loaded_upgr_idx, id);
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

item user::get_loaded_callable_scriptname_item(db::read_tx& ctx, const std::string& full_name)
{
    std::vector<std::string> loaded = all_loaded_items();

    for(auto& id : loaded)
    {
        item next;
        db_disk_load(ctx, next, id);

        if(name + "." + next.get_prop("registered_as") == full_name)
            return next;
    }

    return item();
}

std::vector<item> user::get_all_items(db::read_tx& ctx)
{
    std::vector<std::string> all_items = upgr_idx;

    std::vector<item> ret;

    for(auto& id : all_items)
    {
        item next;

        if(!db_disk_load(ctx, next, id))
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
    if(id == "")
        throw std::runtime_error("Tried to add bad item id");

    if(has_item(id))
        throw std::runtime_error("Already has item");

    upgr_idx.push_back(id);
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

    erase_remove(upgr_idx, id);
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
        mongo_read_proxy ctx = get_global_mongo_user_info_context(thread_id);

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

    overwrite_user_in_db(ctx);

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

    overwrite_user_in_db(ctx);

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

///TODO: THIS IS REALLY BAD AND SLOW
std::vector<item> user::search_my_scripts(mongo_lock_proxy& ctx)
{
    std::vector<item> all_items = db_disk_load_all(ctx, item());

    std::vector<item> ret;

    for(item& i : all_items)
    {
        if(i.get_string("owner") != name)
            continue;

        if(i.get_int("is_script") != 1)
            continue;

        ret.push_back(i);
    }

    return ret;
}

int user::find_num_scripts(mongo_lock_proxy& ctx)
{
    return search_my_scripts(ctx).size();
}

int user::find_num_public_scripts(mongo_lock_proxy& ctx)
{
    auto my_scripts = search_my_scripts(ctx);

    int pub = 0;

    for(item& i : my_scripts)
    {
        if(i.get_int("in_public") == 1)
            pub++;
    }

    return pub;
}

int user::get_default_network_links(int thread_id)
{
    if(!is_npc())
        return 4;

    int base = 4;

    npc_prop_list props;

    {
        mongo_read_proxy ctx = get_global_mongo_npc_properties_context(thread_id);

        if(!db_disk_load(ctx, props, name))
            return 4;
    }

    std::optional<float> val = props.get_prop(npc_info::MAX_CONNECT);

    if(val.has_value())
    {
        if(val.value() > 0)
            base++;
        if(val.value() < 0)
            base--;
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
            create_notification(name, q.notif_on_finish);

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

                        create_notification(name, make_notif_col("-Arrived at " + target_system.name + "-"));

                        #ifdef DEBUG_WARP
                        ///the problem is that we reset the move queue in steal user
                        ///which alters the queue
                        std::cout << "set pos " << get_local_pos() << std::endl;
                        #endif // DEBUG_WARP
                    }
                    else
                    {
                        create_notification(name, make_notif_col("-Could not activate, out of range-"));

                        reset_internal_queue();

                        #ifdef DEBUG_WARP
                        std::cout << "max warp distance exceeded " << my_dist << std::endl;
                        std::cout << "attempted to warp from " << u1.name << " to " << u2.name << std::endl;
                        #endif // DEBUG_WARP
                    }
                }
                else
                {
                    create_notification(name, make_notif_col("-Could not establish link between systems (internal server error?)-"));

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
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(lock_id);

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
                        fiber_sleep(1);
                      });

        for_each_npc([](user& usr)
                     {
                        usr.pump_notifications(-2);
                        fiber_sleep(1);
                     });

        fiber_sleep(10);
    }
}

void user::launch_pump_events_thread()
{
    get_global_fiber_queue().add(event_pumper);
}

std::vector<user> load_users(const std::vector<std::string>& names, int lock_id)
{
    std::vector<user> ret;

    mongo_read_proxy ctx = get_global_mongo_user_info_context(lock_id);

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

    mongo_read_proxy ctx = get_global_mongo_user_info_context(-2);

    for(auto& i : names)
    {
        user usr;

        if(!usr.load_from_db(ctx, i))
            continue;

        ret.push_back(usr);
    }

    return ret;
}
