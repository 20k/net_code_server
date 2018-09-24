#ifndef MONGO_CLEANUP_H_INCLUDED
#define MONGO_CLEANUP_H_INCLUDED

#include <map>
#include <array>

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
