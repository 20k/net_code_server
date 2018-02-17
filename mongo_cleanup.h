#ifndef MONGO_CLEANUP_H_INCLUDED
#define MONGO_CLEANUP_H_INCLUDED

#include <map>

inline
void cleanup_mongo_all();

inline
mongo_lock_proxy get_global_mongo_context(mongo_database_type type, bool destroy = false)
{
    printf("get glob\n");

    static std::mutex no_race;
    std::lock_guard<std::mutex> lk(no_race);

    printf("locked\n");

    static std::map<mongo_database_type, mongo_context*> data;

    if(data[type] == nullptr && !destroy)
    {
        if(data.size() == 0)
            atexit(cleanup_mongo_all);

        data[type] = new mongo_context(type);
    }

    if(destroy)
    {
        for(auto& i : data)
            delete i.second;

        data.clear();

        return mongo_lock_proxy(nullptr);

        //return nullptr;
    }

    return mongo_lock_proxy(data[type]);
}

inline
void cleanup_mongo_all()
{
    ///first argument is irrelevant
    get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE, true);
}

inline
mongo_lock_proxy get_global_mongo_user_accessible_context()
{
    return get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE);
}

inline
mongo_lock_proxy get_global_mongo_user_info_context()
{
    return get_global_mongo_context(mongo_database_type::USER_PROPERTIES);
}

inline
mongo_lock_proxy get_global_mongo_user_items_context()
{
    return get_global_mongo_context(mongo_database_type::USER_ITEMS);
}

inline
mongo_lock_proxy get_global_mongo_global_properties_context()
{
    return get_global_mongo_context(mongo_database_type::GLOBAL_PROPERTIES);
}

#endif // MONGO_CLEANUP_H_INCLUDED
