#ifndef MONGO_CLEANUP_H_INCLUDED
#define MONGO_CLEANUP_H_INCLUDED


inline
void cleanup_mongo_user_accessible();

inline
mongo_context* get_global_mongo_user_accessible_context(bool destroy = false)
{
    static mongo_context* ctx = nullptr;

    if(ctx == nullptr)
    {
        ctx = new mongo_context();

        atexit(cleanup_mongo_user_accessible);
    }

    if(destroy)
    {
        if(ctx)
            delete ctx;

        ctx = nullptr;
    }

    return ctx;
}


inline
void cleanup_mongo_user_accessible()
{
    get_global_mongo_user_accessible_context(true);
}


inline
void cleanup_mongo_user_info();

inline mongo_context* get_global_mongo_user_info_context(bool destroy = false)
{
    static mongo_context* ctx = nullptr;

    if(ctx == nullptr)
    {
        ctx = new mongo_context();

        atexit(cleanup_mongo_user_info);
    }

    if(destroy)
    {
        if(ctx)
            delete ctx;

        ctx = nullptr;
    }

    return ctx;
}

inline
void cleanup_mongo_user_info()
{
    get_global_mongo_user_info_context(true);
}

#endif // MONGO_CLEANUP_H_INCLUDED
