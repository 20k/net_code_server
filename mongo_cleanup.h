#ifndef MONGO_CLEANUP_H_INCLUDED
#define MONGO_CLEANUP_H_INCLUDED

inline
void cleanup_mongo_all();

inline
mongo_context* get_global_mongo_context(mongo_database_type type, bool destroy = false)
{
    static std::map<mongo_database_type, mongo_context*> data;

    if(data[type] == nullptr && !destroy)
    {
        if(data.size() == 0)
            atexit(cleanup_mongo_all);

        data[type] = new mongo_context;
    }

    if(destroy)
    {
        for(auto& i : data)
            delete i.second;

        data.clear();

        return nullptr;
    }

    return data[type];
}

inline
void cleanup_mongo_all()
{
    ///first argument is irrelevant
    get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE, true);
}

inline
mongo_context* get_global_mongo_user_accessible_context(bool destroy = false)
{
    return get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE);
}

inline
mongo_context* get_global_mongo_user_info_context(bool destroy = false)
{
    return get_global_mongo_context(mongo_database_type::USER_PROPERTIES);
}

#endif // MONGO_CLEANUP_H_INCLUDED
