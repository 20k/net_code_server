#ifndef MONGO_CLEANUP_H_INCLUDED
#define MONGO_CLEANUP_H_INCLUDED

#include <map>

inline
void cleanup_mongo_all();

inline std::map<mongo_database_type, mongo_context*> mongo_databases;
inline std::mutex mongo_databases_lock;

///if a script were terminated while fetching the global mongo context, everything would break
///ALARM: ALARM:
inline
mongo_lock_proxy get_global_mongo_context(mongo_database_type type, int lock_id, bool destroy = false)
{
    std::lock_guard<std::mutex> lk(mongo_databases_lock);

    if(mongo_databases[type] == nullptr && !destroy)
    {
        if(mongo_databases.size() == 0)
            atexit(cleanup_mongo_all);

        mongo_databases[type] = new mongo_context(type);
    }

    if(destroy)
    {
        for(auto& i : mongo_databases)
        {
            delete i.second;
        }

        mongo_databases.clear();

        return mongo_lock_proxy(nullptr, lock_id);

        //return nullptr;
    }

    return mongo_lock_proxy(mongo_databases[type], lock_id);
}

inline
void cleanup_mongo_all()
{
    ///first argument is irrelevant
    get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE, true);
}

inline
mongo_lock_proxy get_global_mongo_user_accessible_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE, lock_id);
}

inline
mongo_lock_proxy get_global_mongo_user_info_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_PROPERTIES, lock_id);
}

inline
mongo_lock_proxy get_global_mongo_user_items_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_ITEMS, lock_id);
}

inline
mongo_lock_proxy get_global_mongo_global_properties_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::GLOBAL_PROPERTIES, lock_id);
}

inline
mongo_lock_proxy get_global_mongo_chat_channels_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::CHAT_CHANNELS, lock_id);
}

#if 0
inline
mongo_lock_proxy get_global_mongo_user_auth_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_AUTH, lock_id);
}
#endif // 0

#endif // MONGO_CLEANUP_H_INCLUDED
