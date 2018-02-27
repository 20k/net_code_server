#ifndef MONGO_CLEANUP_H_INCLUDED
#define MONGO_CLEANUP_H_INCLUDED

#include <map>
#include <array>

inline
void cleanup_mongo_all();

inline std::array<mongo_context*, (int)mongo_database_type::MONGO_COUNT> mongo_databases;

inline
void initialse_mongo_all()
{
    mongoc_init();

    for(int i=0; i < (int)mongo_database_type::MONGO_COUNT; i++)
        mongo_databases[i] = new mongo_context((mongo_database_type)i);

    atexit(cleanup_mongo_all);
}

///if a script were terminated while fetching the global mongo context, everything would break
///ALARM: ALARM:
inline
mongo_lock_proxy get_global_mongo_context(mongo_database_type type, int lock_id, bool destroy = false)
{
    if(destroy)
    {
        for(auto& i : mongo_databases)
        {
            delete i;
            i = nullptr;
        }

        return mongo_lock_proxy(nullptr, lock_id);
    }

    return mongo_lock_proxy(mongo_databases[(int)type], lock_id);
}

inline
void cleanup_mongo_all()
{
    ///first argument is irrelevant
    get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE, true);

    mongoc_cleanup();
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

/*inline
std::vector<int> any_mongo_locks()
{
    for(auto& i : mongo_databases)
    {

    }
}*/

#if 0
inline
mongo_lock_proxy get_global_mongo_user_auth_context(int lock_id)
{
    return get_global_mongo_context(mongo_database_type::USER_AUTH, lock_id);
}
#endif // 0

#endif // MONGO_CLEANUP_H_INCLUDED
