#include "db_interfaceable.hpp"
#include "mongo.hpp"

/*template<char... name>
bool db_interfaceable<name>::load_from_db(mongo_lock_proxy& ctx, const std::string& id)
{
    if(!exists(ctx, id))
        return false;

    json to_find;
    to_find[key_name.key] = id;

    try
    {
        auto found = ctx->find_json(ctx->last_collection, )
    }
    catch(...)
    {
        return false;
    }
}*/

#include <secret/low_level_structure.hpp>
#include <secret/npc_manager.hpp>
#include "scheduled_tasks.hpp"
#include "item.hpp"

/*template global_generic_cache<low_level_structure> caches::this_cache<low_level_structure>;
template global_generic_cache<npc_prop_list> caches::this_cache<npc_prop_list>;
template global_generic_cache<task_data_db> caches::this_cache<task_data_db>;
template global_generic_cache<item> caches::this_cache<item>;*/
