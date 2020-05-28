#include "serialisables.hpp"
#include <networking/serialisable.hpp>
#include <networking/serialisable_msgpack.hpp>
#include "user.hpp"
#include "auth.hpp"
#include <secret/node.hpp>
#include <secret/npc_manager.hpp>
#include "event_manager.hpp"
#include "scheduled_tasks.hpp"
#include "quest_manager.hpp"
#include <secret/low_level_structure.hpp>
#include "item.hpp"
#include "chat_channels.hpp"
#include "db_storage_backend_lmdb.hpp"
#include "mongo.hpp"
#include "timestamped_event_queue.hpp"
#include "entity_manager.hpp"
#include <secret/solar_system.hpp>

template<typename T>
DEFINE_MSG_FSERIALISE(event_queue::timestamp_event_base<T>)
{
    SETUP_MSG_FSERIALISE_SIMPLE(7);

    DO_MSG_FSERIALISE_SIMPLE(timestamp);
    DO_MSG_FSERIALISE_SIMPLE(quantity);
    DO_MSG_FSERIALISE_SIMPLE(originator_script);
    DO_MSG_FSERIALISE_SIMPLE(originator_script_id);
    DO_MSG_FSERIALISE_SIMPLE(entity_id);
    DO_MSG_FSERIALISE_SIMPLE(callback);
    DO_MSG_FSERIALISE_SIMPLE(fired);
}

template<typename T>
DEFINE_MSG_FSERIALISE(event_queue::event_stack<T>)
{
    SETUP_MSG_FSERIALISE_SIMPLE(1);

    DO_MSG_FSERIALISE_SIMPLE(events);
}

DEFINE_MSG_FSERIALISE(user_limit)
{
    SETUP_MSG_FSERIALISE_SIMPLE(2);

    DO_MSG_FSERIALISE_SIMPLE(data);
    DO_MSG_FSERIALISE_SIMPLE(time_at);
}

DEFINE_MSG_FSERIALISE(timestamped_position)
{
    SETUP_MSG_FSERIALISE_SIMPLE(5);

    DO_MSG_FSERIALISE_SIMPLE(type);
    DO_MSG_FSERIALISE_SIMPLE(timestamp);
    DO_MSG_FSERIALISE_SIMPLE(position);
    DO_MSG_FSERIALISE_SIMPLE(notif_on_finish);
    DO_MSG_FSERIALISE_SIMPLE(system_to_arrive_at);
}

DEFINE_MSG_FSERIALISE(timestamp_move_queue)
{
    SETUP_MSG_FSERIALISE_SIMPLE(1);

    DO_MSG_FSERIALISE_SIMPLE(timestamp_queue);
}

DEFINE_MSG_FSERIALISE(user)
{
    SETUP_MSG_FSERIALISE_SIMPLE(14);

    DO_MSG_FSERIALISE_SIMPLE(name);
    DO_MSG_FSERIALISE_SIMPLE(cash);
    DO_MSG_FSERIALISE_SIMPLE(auth_hex);
    DO_MSG_FSERIALISE_SIMPLE(upgr_idx);
    DO_MSG_FSERIALISE_SIMPLE(loaded_upgr_idx);
    DO_MSG_FSERIALISE_SIMPLE(initial_connection_setup);
    DO_MSG_FSERIALISE_SIMPLE(call_stack);
    DO_MSG_FSERIALISE_SIMPLE(owner_list);
    DO_MSG_FSERIALISE_SIMPLE(users_i_have_access_to);

    try
    {
        DO_MSG_FSERIALISE_SIMPLE(user_limits);
    }
    catch(...)
    {

    }

    DO_MSG_FSERIALISE_SIMPLE(pos);
    DO_MSG_FSERIALISE_SIMPLE(has_local_pos);
    DO_MSG_FSERIALISE_SIMPLE(hacked_progress);
    DO_MSG_FSERIALISE_SIMPLE(move_queue);
}

DEFINE_MSG_FSERIALISE(auth)
{
    SETUP_MSG_FSERIALISE_SIMPLE(3);

    DO_MSG_FSERIALISE_SIMPLE(auth_token_hex);
    DO_MSG_FSERIALISE_SIMPLE(steam_id);
    DO_MSG_FSERIALISE_SIMPLE(users);
}

DEFINE_MSG_FSERIALISE(user_log_fragment)
{
    SETUP_MSG_FSERIALISE_SIMPLE(4);

    DO_MSG_FSERIALISE_SIMPLE(col);
    DO_MSG_FSERIALISE_SIMPLE(key);
    DO_MSG_FSERIALISE_SIMPLE(text);
    DO_MSG_FSERIALISE_SIMPLE(hide_key);
}

DEFINE_MSG_FSERIALISE(user_log)
{
    SETUP_MSG_FSERIALISE_SIMPLE(1);

    DO_MSG_FSERIALISE_SIMPLE(frags);
}

DEFINE_MSG_FSERIALISE(user_node)
{
    SETUP_MSG_FSERIALISE_SIMPLE(9);

    DO_MSG_FSERIALISE_SIMPLE(type);
    DO_MSG_FSERIALISE_SIMPLE(state);
    DO_MSG_FSERIALISE_SIMPLE(max_locks);
    DO_MSG_FSERIALISE_SIMPLE(owned_by);
    DO_MSG_FSERIALISE_SIMPLE(unique_id);
    DO_MSG_FSERIALISE_SIMPLE(attached_locks);
    DO_MSG_FSERIALISE_SIMPLE(connected_to);
    DO_MSG_FSERIALISE_SIMPLE(new_logs);
    DO_MSG_FSERIALISE_SIMPLE(time_last_breached_at_s);

    if(!ctx.encode)
    {
        while(me.new_logs.size() > MAX_LOGS)
            me.new_logs.erase(me.new_logs.begin());
    }
}

DEFINE_MSG_FSERIALISE(user_nodes)
{
    SETUP_MSG_FSERIALISE_SIMPLE(2);

    DO_MSG_FSERIALISE_SIMPLE(nodes);
    DO_MSG_FSERIALISE_SIMPLE(owned_by);
}

DEFINE_MSG_FSERIALISE(npc_prop)
{
    SETUP_MSG_FSERIALISE_SIMPLE(2);

    DO_MSG_FSERIALISE_SIMPLE(val);
    DO_MSG_FSERIALISE_SIMPLE(cap);
}

DEFINE_MSG_FSERIALISE(npc_prop_list)
{
    SETUP_MSG_FSERIALISE_SIMPLE(3);

    DO_MSG_FSERIALISE_SIMPLE(name);
    DO_MSG_FSERIALISE_SIMPLE(props);
    DO_MSG_FSERIALISE_SIMPLE(wh_puzz_set);
}

DEFINE_MSG_FSERIALISE(event_impl)
{
    SETUP_MSG_FSERIALISE_SIMPLE(4);

    DO_MSG_FSERIALISE_SIMPLE(id);
    DO_MSG_FSERIALISE_SIMPLE(user_name);
    DO_MSG_FSERIALISE_SIMPLE(unique_event_tag);
    DO_MSG_FSERIALISE_SIMPLE(complete);
}

DEFINE_MSG_FSERIALISE(task_data_db)
{
    SETUP_MSG_FSERIALISE_SIMPLE(6);

    DO_MSG_FSERIALISE_SIMPLE(start_time_s);
    DO_MSG_FSERIALISE_SIMPLE(end_time_s);
    DO_MSG_FSERIALISE_SIMPLE(called_callback);
    DO_MSG_FSERIALISE_SIMPLE(type);
    DO_MSG_FSERIALISE_SIMPLE(udata);
    DO_MSG_FSERIALISE_SIMPLE(count_offset);
}

DEFINE_MSG_FSERIALISE(quest)
{
    SETUP_MSG_FSERIALISE_SIMPLE(6);

    DO_MSG_FSERIALISE_SIMPLE(id);
    DO_MSG_FSERIALISE_SIMPLE(user_for);
    DO_MSG_FSERIALISE_SIMPLE(name);
    DO_MSG_FSERIALISE_SIMPLE(description);
    DO_MSG_FSERIALISE_SIMPLE(quest_data);
    DO_MSG_FSERIALISE_SIMPLE(run_on_complete);
}

DEFINE_MSG_FSERIALISE(low_level_structure)
{
    SETUP_MSG_FSERIALISE_SIMPLE(6);

    DO_MSG_FSERIALISE_SIMPLE(name);
    DO_MSG_FSERIALISE_SIMPLE(px);
    DO_MSG_FSERIALISE_SIMPLE(py);
    DO_MSG_FSERIALISE_SIMPLE(pz);
    DO_MSG_FSERIALISE_SIMPLE(radius);
    DO_MSG_FSERIALISE_SIMPLE(user_list);
}

DEFINE_MSG_FSERIALISE(item)
{
    SETUP_MSG_FSERIALISE_SIMPLE(2);

    DO_MSG_FSERIALISE_SIMPLE(item_id);
    DO_MSG_FSERIALISE_SIMPLE(data);
}

DEFINE_MSG_FSERIALISE(playspace_network_link)
{
    SETUP_MSG_FSERIALISE_SIMPLE(3);

    DO_MSG_FSERIALISE_SIMPLE(name);
    DO_MSG_FSERIALISE_SIMPLE(links);
    DO_MSG_FSERIALISE_SIMPLE(strengths);
}

DEFINE_MSG_FSERIALISE(chat_channel)
{
    SETUP_MSG_FSERIALISE_SIMPLE(4);

    DO_MSG_FSERIALISE_SIMPLE(channel_name);
    DO_MSG_FSERIALISE_SIMPLE(password);
    DO_MSG_FSERIALISE_SIMPLE(user_list);
    DO_MSG_FSERIALISE_SIMPLE(history);
}

DEFINE_MSG_FSERIALISE(chat_message)
{
    SETUP_MSG_FSERIALISE_SIMPLE(6);

    DO_MSG_FSERIALISE_SIMPLE(id);
    DO_MSG_FSERIALISE_SIMPLE(time_ms);
    DO_MSG_FSERIALISE_SIMPLE(originator);
    DO_MSG_FSERIALISE_SIMPLE(msg);
    DO_MSG_FSERIALISE_SIMPLE(recipient_list);
    DO_MSG_FSERIALISE_SIMPLE(sent_to_client);
}

DEFINE_MSG_FSERIALISE(entity::ship)
{
    SETUP_MSG_FSERIALISE_SIMPLE(5);

    DO_MSG_FSERIALISE_SIMPLE(id);
    DO_MSG_FSERIALISE_SIMPLE(solar_system_id);
    DO_MSG_FSERIALISE_SIMPLE(position);
    DO_MSG_FSERIALISE_SIMPLE(system_current);
    DO_MSG_FSERIALISE_SIMPLE(system_max);
}

DEFINE_MSG_FSERIALISE(space::solar_system)
{
    SETUP_MSG_FSERIALISE_SIMPLE(4);

    DO_MSG_FSERIALISE_SIMPLE(id);
    DO_MSG_FSERIALISE_SIMPLE(name);
    DO_MSG_FSERIALISE_SIMPLE(seclevel);
    DO_MSG_FSERIALISE_SIMPLE(ships);
}

template<typename T>
std::string any_to_string(const T& in)
{
    if constexpr(std::is_same_v<T, std::string>)
        return in;
    else
        return std::to_string(in);
}

#ifdef OLD_DB
using read_write_type = mongo_lock_proxy;
#else
using read_write_type = db::read_write_tx;
#endif

#ifdef OLD_DB
using read_type = mongo_lock_proxy
#else
using read_type = db::read_tx;
#endif // OLD_DB

template<typename T, typename U>
bool db_load_impl(T& val, read_type& ctx, const std::string& key_name, const U& key_val, int db_id)
{
    #ifdef OLD_DB
    if(!db_exists_impl(ctx, key_name, key_val))
        return false;

    nlohmann::json fetch;
    fetch[key_name] = key_val;
    std::vector<nlohmann::json> found = ctx.ctx.find_json_new(fetch, {});

    if(found.size() != 1)
        return false;

    val = T();

    deserialise(found[0], val, serialise_mode::DISK);

    if constexpr(std::is_same_v<T, item>)
    {
        val.fix();
    }

    return true;
    #else
    std::optional<db::data> data = ctx.read(db_id, any_to_string(key_val));

    if(!data.has_value())
        return false;

    val = deserialise_msg<T>(data.value().data_view);

    return true;
    #endif
}

template<typename U>
bool db_exists_impl(read_type& ctx, const std::string& key_name, const U& key_val, int db_id)
{
    #ifdef OLD_DB
    nlohmann::json to_find;
    to_find[key_name] = key_val;
    return ctx.ctx.find_json_new(to_find, nlohmann::json()).size() == 1;
    #else
    return ctx.read(db_id, any_to_string(key_val)).has_value();
    #endif
}

template<typename U>
void db_remove_impl(read_write_type& ctx, const std::string& key_name, const U& key_val, int db_id)
{
    #ifdef OLD_DB
    nlohmann::json to_remove;
    to_remove[key_name] = key_val;
    ctx.ctx.remove_json_many_new(to_remove);
    #else
    ctx.del(db_id, any_to_string(key_val));
    #endif
}

template<typename T, typename U>
void db_overwrite_impl(T& val, read_write_type& ctx, const std::string& key_name, const U& key_val, int db_id)
{
    #ifdef OLD_DB
    if(!db_exists_impl(ctx, key_name, key_val))
    {
        ctx.ctx.insert_json_one_new(serialise(val, serialise_mode::DISK));
    }
    else
    {
        nlohmann::json selector;
        selector[key_name] = key_val;

        nlohmann::json to_set;
        to_set["$set"] = serialise(val, serialise_mode::DISK);

        ctx.ctx.update_json_one_new(selector, to_set);
    }
    #else
    std::string vals = serialise_msg(val);

    if(vals.size() == 0)
        throw std::runtime_error("Vals.size() == 0");

    std::string_view view((const char*)&vals[0], vals.size());

    ctx.write(db_id, any_to_string(key_val), view);
    #endif
}

template<typename T>
std::vector<T> db_load_all_impl(read_type& ctx, const std::string& key_name, int db_id)
{
    #ifdef OLD_DB
    nlohmann::json exist;
    exist["$exists"] = true;

    nlohmann::json to_find;
    to_find[key_name] = exist;

    auto found = ctx.ctx.find_json_new(to_find, nlohmann::json());

    std::vector<T> ret;
    ret.reserve(found.size());

    for(auto& i : found)
    {
        T& next = ret.emplace_back();

        deserialise(i, next, serialise_mode::DISK);

        if constexpr(std::is_same_v<T, item>)
        {
            next.fix();
        }
    }

    return ret;
    #else
    std::vector<db::data> data = ctx.read_all(db_id);

    std::vector<T> ret;

    for(db::data& dat : data)
    {
        T& val = ret.emplace_back();

        val = deserialise_msg<T>(dat.data_view);
    }

    return ret;
    #endif
}

template<typename T>
void db_remove_all_impl(read_write_type& ctx, const std::string& key_name, int db_id)
{
    #ifdef OLD_DB
    nlohmann::json exist;
    exist["$exists"] = true;

    nlohmann::json to_find;
    to_find[key_name] = exist;

    ctx.ctx.remove_json_many_new(to_find);
    #else
    ctx.drop(db_id);
    #endif
}

DEFINE_GENERIC_DB(npc_prop_list, std::string, name, mongo_database_type::NPC_PROPERTIES);
DEFINE_GENERIC_DB(event_impl, std::string, id, mongo_database_type::EVENT_MANAGER);
DEFINE_GENERIC_DB(task_data_db, std::string, id, mongo_database_type::SCHEDULED_TASK);
DEFINE_GENERIC_DB(quest, std::string, id, mongo_database_type::QUEST_MANAGER);
DEFINE_GENERIC_DB(low_level_structure, std::string, name, mongo_database_type::LOW_LEVEL_STRUCTURE);
DEFINE_GENERIC_DB(item, std::string, item_id, mongo_database_type::USER_ITEMS);
DEFINE_GENERIC_DB(user, std::string, name, mongo_database_type::USER_PROPERTIES);
DEFINE_GENERIC_DB(playspace_network_link, std::string, name, mongo_database_type::NETWORK_PROPERTIES);
DEFINE_GENERIC_DB(auth, std::string, auth_token_hex, mongo_database_type::GLOBAL_PROPERTIES);
DEFINE_GENERIC_DB(chat_channel, std::string, channel_name, mongo_database_type::CHAT_CHANNEL_PROPERTIES);
DEFINE_GENERIC_DB(chat_message, size_t, id, mongo_database_type::CHAT_MESSAGES);
DEFINE_GENERIC_DB(user_nodes, std::string, owned_by, mongo_database_type::NODE_PROPERTIES);
DEFINE_GENERIC_DB(entity::ship, uint32_t, id, mongo_database_type::SHIP_PROPERTIES);
DEFINE_GENERIC_DB(space::solar_system, uint32_t, id, mongo_database_type::SOLAR_SYSTEM_PROPERTIES);
