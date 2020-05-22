#include "serialisables.hpp"
#include <networking/serialisable.hpp>
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

template<typename T>
DEFINE_SERIALISE_FUNCTION(event_queue::timestamp_event_base<T>)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(quantity);
    DO_FSERIALISE(timestamp);
    DO_FSERIALISE(originator_script);
    DO_FSERIALISE(originator_script_id);
}

template<typename T>
DEFINE_SERIALISE_FUNCTION(event_queue::event_stack<T>)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(events);
}

DEFINE_SERIALISE_FUNCTION(user_limit)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(data);
    DO_FSERIALISE(time_at);
}

DEFINE_SERIALISE_FUNCTION(timestamped_position)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(type);
    DO_FSERIALISE(timestamp);
    DO_FSERIALISE(position);
    DO_FSERIALISE(notif_on_finish);
    DO_FSERIALISE(system_to_arrive_at);
}

DEFINE_SERIALISE_FUNCTION(timestamp_move_queue)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(timestamp_queue);
}

DEFINE_SERIALISE_FUNCTION(user)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(name);
    DO_FSERIALISE(cash);
    DO_FSERIALISE(auth_hex);
    DO_FSERIALISE(upgr_idx);
    DO_FSERIALISE(loaded_upgr_idx);
    DO_FSERIALISE(initial_connection_setup);
    DO_FSERIALISE(call_stack);
    DO_FSERIALISE(owner_list);
    DO_FSERIALISE(users_i_have_access_to);
    try
    {
        DO_FSERIALISE(user_limits);
    }
    catch(...)
    {

    }

    DO_FSERIALISE(pos);
    DO_FSERIALISE(has_local_pos);
    DO_FSERIALISE(hacked_progress);
    DO_FSERIALISE(move_queue);
}

DEFINE_SERIALISE_FUNCTION(auth)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(auth_token_hex);
    DO_FSERIALISE(steam_id);
    DO_FSERIALISE(users);
}

DEFINE_SERIALISE_FUNCTION(user_log_fragment)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(col);
    DO_FSERIALISE(key);
    DO_FSERIALISE(text);
    DO_FSERIALISE(hide_key);
}

DEFINE_SERIALISE_FUNCTION(user_log)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(frags);
}

DEFINE_SERIALISE_FUNCTION(user_node)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(type);
    DO_FSERIALISE(state);
    DO_FSERIALISE(max_locks);
    DO_FSERIALISE(owned_by);
    DO_FSERIALISE(unique_id);
    DO_FSERIALISE(attached_locks);
    DO_FSERIALISE(connected_to);
    DO_FSERIALISE(new_logs);
    DO_FSERIALISE(time_last_breached_at_s);

    if(ctx.serialisation && !ctx.encode)
    {
        while(me->new_logs.size() > MAX_LOGS)
            me->new_logs.erase(me->new_logs.begin());
    }
}

DEFINE_SERIALISE_FUNCTION(user_nodes)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(nodes);
    DO_FSERIALISE(owned_by);
}

DEFINE_SERIALISE_FUNCTION(npc_prop)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(val);
    DO_FSERIALISE(cap);
}

DEFINE_SERIALISE_FUNCTION(npc_prop_list)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(name);
    DO_FSERIALISE(props);
    DO_FSERIALISE(wh_puzz_set);
}

DEFINE_SERIALISE_FUNCTION(event_impl)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(id);
    DO_FSERIALISE(user_name);
    DO_FSERIALISE(unique_event_tag);
    DO_FSERIALISE(complete);
}

DEFINE_SERIALISE_FUNCTION(task_data_db)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(start_time_s);
    DO_FSERIALISE(end_time_s);
    DO_FSERIALISE(called_callback);
    DO_FSERIALISE(type);
    DO_FSERIALISE(udata);
    DO_FSERIALISE(count_offset);
}

DEFINE_SERIALISE_FUNCTION(quest)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(id);
    DO_FSERIALISE(user_for);
    DO_FSERIALISE(name);
    DO_FSERIALISE(description);
    DO_FSERIALISE(quest_data);
    DO_FSERIALISE(run_on_complete);
}

DEFINE_SERIALISE_FUNCTION(low_level_structure)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(name);
    DO_FSERIALISE(px);
    DO_FSERIALISE(py);
    DO_FSERIALISE(pz);
    DO_FSERIALISE(radius);
    DO_FSERIALISE(user_list);
}

DEFINE_SERIALISE_FUNCTION(item)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(item_id);
    DO_FSERIALISE(data);
}

DEFINE_SERIALISE_FUNCTION(playspace_network_link)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(name);
    DO_FSERIALISE(links);
    DO_FSERIALISE(strengths);
}

DEFINE_SERIALISE_FUNCTION(chat_channel)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(channel_name);
    DO_FSERIALISE(password);
    DO_FSERIALISE(user_list);
    DO_FSERIALISE(history);
}

DEFINE_SERIALISE_FUNCTION(chat_message)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(id);
    DO_FSERIALISE(time_ms);
    DO_FSERIALISE(originator);
    DO_FSERIALISE(msg);
    DO_FSERIALISE(recipient_list);
    DO_FSERIALISE(sent_to_client);
}

DEFINE_SERIALISE_FUNCTION(entity::ship)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(id);
    DO_FSERIALISE(position);
    DO_FSERIALISE(system_current);
    DO_FSERIALISE(system_max);
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

    val = T();

    nlohmann::json json = nlohmann::json::from_cbor(data.value().data_view);

    deserialise(json, val, serialise_mode::DISK);

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

    //std::cout << "WRITING KEYVAL " << key_val << " FOR " << key_name << std::endl;

    std::vector<uint8_t> vals = nlohmann::json::to_cbor(serialise(val, serialise_mode::DISK));

    if(vals.size() == 0)
        throw std::runtime_error("Vals.size() == 0");

    std::string_view view((const char*)&vals[0], vals.size());

    ctx.write(db_id, any_to_string(key_val), view);
    #endif
}

template<>
void db_overwrite_impl<item, std::string>(item& val, read_write_type& ctx, const std::string& key_name, const std::string& item_id, int db_id)
{
    #ifdef OLD_DB
    nlohmann::json as_ser = serialise(val, serialise_mode::DISK);

    nlohmann::json hacky_data = val.data;
    hacky_data["item_id"] = val.item_id;

    for(auto& i : hacky_data.items())
    {
        as_ser[i.key()] = i.value();
    }

    as_ser["item_id"] = val.item_id;

    if(!db_exists_impl(ctx, key_name, item_id))
    {
        ctx.ctx.insert_json_one_new(as_ser);
    }
    else
    {
        nlohmann::json selector;
        selector[key_name] = item_id;

        nlohmann::json to_set;
        to_set["$set"] = as_ser;

        ctx.ctx.update_json_one_new(selector, to_set);
    }
    #else
    nlohmann::json as_ser = serialise(val, serialise_mode::DISK);

    nlohmann::json hacky_data = val.data;
    hacky_data["item_id"] = val.item_id;

    for(auto& i : hacky_data.items())
    {
        as_ser[i.key()] = i.value();
    }

    as_ser["item_id"] = val.item_id;

    std::vector<uint8_t> vals = nlohmann::json::to_cbor(as_ser);

    if(vals.size() == 0)
        throw std::runtime_error("Vals.size() == 0");

    std::string_view view((const char*)&vals[0], vals.size());

    ctx.write(db_id, any_to_string(item_id), view);
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

        nlohmann::json json = nlohmann::json::from_cbor(dat.data_view);

        deserialise(json, val, serialise_mode::DISK);
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
