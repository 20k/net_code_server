#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include <map>
#include <atomic>
#include <thread>
#include "perfmon.hpp"

#include <nlohmann/json.hpp>
#include "db_storage_backend.hpp"

//#define DEADLOCK_DETECTION

enum class mongo_database_type
{
    USER_ACCESSIBLE,
    USER_PROPERTIES,
    USER_ITEMS,
    GLOBAL_PROPERTIES,
    PENDING_NOTIFS,
    CHAT_CHANNEL_PROPERTIES,
    NODE_PROPERTIES,
    NPC_PROPERTIES,
    NETWORK_PROPERTIES,
    SCHEDULED_TASK,
    LOW_LEVEL_STRUCTURE,
    QUEST_MANAGER,
    EVENT_MANAGER,
    MEMORY_CORE,
    MONGO_COUNT
};

//#define USE_STD_MUTEX

struct lock_internal
{
    size_t locked_by = -1;
    #ifdef DEADLOCK_DETECTION
    std::thread::id locked_by_tid;
    std::string locked_by_debug;
    #endif // DEADLOCK_DETECTION

    #ifndef USE_STD_MUTEX
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
    #else
    safe_mutex mut_lock;
    #endif // USE_STD_MUTEX

    void lock(const std::string& debug_info, size_t who);
    void unlock();
};

///hmm
///maybe move most of mongo context into lock proxy
///and get the client in a thread safe way
struct mongo_context
{
    std::string last_db = "";
    mongo_database_type last_db_type = mongo_database_type::MONGO_COUNT;

    std::string default_collection = "";

    std::vector<std::string> all_collections;

    ///thread safety of below map
    ///make timed
    std::recursive_timed_mutex map_lock;

    ///this isn't for thread safety, this is for marshalling db access
    std::map<std::string, lock_internal> per_collection_lock;

    static inline std::map<std::thread::id, std::atomic_int> thread_counter;
    static inline std::mutex thread_lock;

    bool is_fixed = false;

    ///need to run everything through a blacklist
    ///can probably just blacklist json

    ///if we ever have to add another db, make this fully data driven with structs and definitions and the like
    mongo_context(mongo_database_type type);

    void map_lock_for();

    void make_lock(const std::string& debug_info, const std::string& collection, size_t who);
    void make_unlock(const std::string& collection);
};

int* tls_get_thread_id_storage_hack();
int* tls_get_print_performance_diagnostics();
int* tls_get_should_throw();
int* tls_get_holds_lock();

struct database_read_interface
{
    mongo_context* ctx = nullptr;
    db_storage_backend backend;
    std::string last_collection;

    void change_collection_unsafe(const std::string& coll, bool force_change = false);

    std::vector<nlohmann::json> find_json_new(const nlohmann::json& json, const nlohmann::json& opts);

    database_read_interface(mongo_context* fctx);
};

struct database_read_write_interface : database_read_interface
{
    void insert_json_one_new(const nlohmann::json& json);
    void update_json_many_new(const nlohmann::json& selector, const nlohmann::json& update);
    void update_json_one_new(const nlohmann::json& selector, const nlohmann::json& update);
    void remove_json_many_new(const nlohmann::json& json);

    database_read_write_interface(mongo_context* fctx);
};

struct mongo_shim
{
    mongo_context* ctx = nullptr;
    int lock_id = -1;

    mongo_shim(mongo_context* fctx, int lock_id);
};

struct mongo_lock_proxy
{
    database_read_write_interface ctx;

    size_t ilock_id = 0;
    int friendly_id = -1;

    bool has_lock = false;
    bool should_lock = true;

    perfmon perf;

    mongo_lock_proxy(const mongo_shim& shim, bool lock = true);
    mongo_lock_proxy(const mongo_lock_proxy&) = delete;
    ~mongo_lock_proxy();

    void change_collection(const std::string& coll, bool force_change = false);

    void lock();
    void unlock();

    database_read_write_interface* operator->();
};

struct mongo_nolock_proxy : mongo_lock_proxy
{
    mongo_nolock_proxy(const mongo_shim& shim);
};

//https://stackoverflow.com/questions/30166706/c-convert-simple-values-to-string
template<typename T>
inline
typename std::enable_if<std::is_fundamental<T>::value, std::string>::type stringify_hack(const T& t)
{
    return std::to_string(t);
}

template<typename T>
inline
typename std::enable_if<!std::is_fundamental<T>::value, std::string>::type  stringify_hack(const T& t)
{
    return std::string(t);
}

///ok, support for arrays is now non negotiable
struct mongo_requester
{
    std::map<std::string, nlohmann::json> props;

    std::map<std::string, int> sort_on;
    std::map<std::string, int> exists_check;

    bool has_prop(const std::string& str) const
    {
        return props.find(str) != props.end();
    }

    ///uuh sure ok, this is super dodgy
    nlohmann::json get_prop_as_array(const std::string& str)
    {
        if(!has_prop(str))
            return std::vector<std::string>();

        return props[str];
    }

    int64_t get_prop_as_integer(const std::string& str) const
    {
        if(!has_prop(str))
            return int64_t();

        return props.at(str);
    }

    uint64_t get_prop_as_uinteger(const std::string& str) const
    {
        if(!has_prop(str))
            return uint64_t();

        return props.at(str);
    }

    nlohmann::json get_prop(const std::string& str)
    {
        if(!has_prop(str))
            return nlohmann::json();

        return props.at(str);
    }

    template<typename T>
    void set_prop(const std::string& key, const T& value)
    {
        props[key] = value;
    }

    void set_prop_sort_on(const std::string& key, int dir)
    {
        sort_on[key] = dir;
    }

    std::vector<mongo_requester> fetch_from_db(mongo_lock_proxy& ctx);

    void insert_in_db(mongo_lock_proxy& ctx);

    nlohmann::json get_all_properties_json();

    void update_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to);
    void update_one_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to);

    void remove_all_from_db(mongo_lock_proxy& ctx);
};

template<>
inline
void mongo_requester::set_prop(const std::string& key, const bool& value)
{
    props[key] = (int)value;
}

inline
std::vector<nlohmann::json> fetch_from_db(mongo_lock_proxy& ctx, const nlohmann::json& fnd, nlohmann::json proj = {})
{
    return ctx->find_json_new(fnd, proj);
}

inline
void remove_all_from_db(mongo_lock_proxy& ctx, const nlohmann::json& rem)
{
    ctx->remove_json_many_new(rem);
}

inline
void update_in_db_if_exact(mongo_lock_proxy& ctx, const nlohmann::json& to_select, const nlohmann::json& to_update)
{
    nlohmann::json to_set;

    to_set["$set"] = to_update;

    ctx->update_json_many_new(to_select, to_set);
}

inline
void insert_in_db(mongo_lock_proxy& ctx, const nlohmann::json& to_insert)
{
    ctx->insert_json_one_new(to_insert);
}

extern std::array<mongo_context*, (int)mongo_database_type::MONGO_COUNT> mongo_databases;

///if a script were terminated while fetching the global mongo context, everything would break
///ALARM: ALARM:
inline
mongo_shim get_global_mongo_context(mongo_database_type type, int lock_id, bool destroy = false)
{
    if(destroy)
    {
        for(auto& i : mongo_databases)
        {
            delete i;
            i = nullptr;
        }

        return mongo_shim(nullptr, lock_id);
    }

    return mongo_shim(mongo_databases[(int)type], lock_id);
}

void initialse_db_all();
void cleanup_db_all();

//bson_t* make_bson_default();
//void destroy_bson_default(bson_t* t);

inline
mongo_shim get_global_mongo_user_accessible_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE, lock_id);
}

inline
mongo_shim get_global_mongo_user_info_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_PROPERTIES, lock_id);
}

inline
mongo_shim get_global_mongo_user_items_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_ITEMS, lock_id);
}

inline
mongo_shim get_global_mongo_global_properties_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::GLOBAL_PROPERTIES, lock_id);
}

/*inline
mongo_lock_proxy get_global_mongo_chat_channels_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::CHAT_CHANNELS, lock_id);
}*/

inline
mongo_shim get_global_mongo_pending_notifs_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::PENDING_NOTIFS, lock_id);
}

inline
mongo_shim get_global_mongo_chat_channel_propeties_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::CHAT_CHANNEL_PROPERTIES, lock_id);
}

inline
mongo_shim get_global_mongo_node_properties_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::NODE_PROPERTIES, lock_id);
}

inline
mongo_shim get_global_mongo_npc_properties_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::NPC_PROPERTIES, lock_id);
}

inline
mongo_shim get_global_mongo_network_properties_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::NETWORK_PROPERTIES, lock_id);
}

inline
mongo_shim get_global_mongo_scheduled_task_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::SCHEDULED_TASK, lock_id);
}

inline
mongo_shim get_global_mongo_low_level_structure_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::LOW_LEVEL_STRUCTURE, lock_id);
}

inline
mongo_shim get_global_mongo_quest_manager_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::QUEST_MANAGER, lock_id);
}

inline
mongo_shim get_global_mongo_event_manager_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::EVENT_MANAGER, lock_id);
}

inline
mongo_shim get_global_mongo_memory_core_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::MEMORY_CORE, lock_id);
}

#endif // MONGO_HPP_INCLUDED
