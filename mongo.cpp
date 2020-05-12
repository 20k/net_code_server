#include "mongo.hpp"
#include <nlohmann/json.hpp>

#include <chrono>
#include <set>
#include "logging.hpp"

#ifdef DEADLOCK_DETECTION
#include <boost/stacktrace.hpp>
#endif // DEADLOCK_DETECTION

#include <thread>
#include "stacktrace.hpp"
#include "safe_thread.hpp"
#include <libncclient/nc_util.hpp>

#include "rate_limiting.hpp"
#include "tls.hpp"

void lock_internal::lock(const std::string& debug_info, size_t who)
{
    mut_lock.lock();

    locked_by = who;
}

void lock_internal::unlock()
{
    #ifdef DEADLOCK_DETECTION
    locked_by_debug = "";
    #endif // DEADLOCK_DETECTION
    locked_by = 0;

    mut_lock.unlock();
}

mongo_context::mongo_context(mongo_database_type type)
{
    std::string db = "Err";

    last_db_type = type;

    if(type == mongo_database_type::USER_ACCESSIBLE)
    {
        db = "user_dbs";
    }

    if(type == mongo_database_type::USER_PROPERTIES)
    {
        db = "user_properties";
    }

    if(type == mongo_database_type::USER_ITEMS)
    {
        db = "user_items";
    }

    if(type == mongo_database_type::GLOBAL_PROPERTIES)
    {
        db = "global_properties";
    }

    if(type == mongo_database_type::PENDING_NOTIFS)
    {
        db = "pending_notifs";
    }

    if(type == mongo_database_type::CHAT_CHANNEL_PROPERTIES)
    {
        db = "chat_channel_properties";
    }

    if(type == mongo_database_type::NODE_PROPERTIES)
    {
        db = "node_properties";
    }

    if(type == mongo_database_type::NPC_PROPERTIES)
    {
        db = "npc_properties";
    }

    if(type == mongo_database_type::NETWORK_PROPERTIES)
    {
        db = "all_networks";
    }

    std::map<mongo_database_type, std::string> procedural_dbs
    {
        {mongo_database_type::SCHEDULED_TASK, "SCHEDULED_TASK"},
        {mongo_database_type::LOW_LEVEL_STRUCTURE, "LOW_LEVEL_STRUCTURE"},
        {mongo_database_type::QUEST_MANAGER, "QUEST_MANAGER"},
        {mongo_database_type::EVENT_MANAGER, "EVENT_MANAGER"},
    };

    std::map<mongo_database_type, bool> is_fixed_map
    {
        {mongo_database_type::SCHEDULED_TASK, true},
        {mongo_database_type::LOW_LEVEL_STRUCTURE, true},
        {mongo_database_type::QUEST_MANAGER, true},
        {mongo_database_type::EVENT_MANAGER, false},
    };

    for(auto& i : procedural_dbs)
    {
        if(i.first == type)
        {
            db = i.second;

            if(is_fixed_map[i.first])
            {
                default_collection = "all_" + i.second;
                is_fixed = true;
            }
            else
            {
                continue;
            }
        }
    }

    last_db = db;

    /*if(type == mongo_database_type::USER_PROPERTIES)
    {
        change_collection("all_users");

        is_fixed = true;
    }*/

    if(type == mongo_database_type::USER_ITEMS)
    {
        default_collection = "all_items";
        is_fixed = true;
    }

    if(type == mongo_database_type::GLOBAL_PROPERTIES)
    {
        default_collection = "global_properties";
        is_fixed = true;
    }

    if(type == mongo_database_type::PENDING_NOTIFS)
    {
        default_collection = "all_notifs";
        is_fixed = true;
    }

    if(type == mongo_database_type::CHAT_CHANNEL_PROPERTIES)
    {
        default_collection = "all_channel_properties";
        is_fixed = true;
    }

    /*if(type == mongo_database_type::NODE_PROPERTIES)
    {
        default_collection = "all_nodes";
        is_fixed = true;
    }*/

    if(type == mongo_database_type::NPC_PROPERTIES)
    {
        default_collection = "all_npcs";
        is_fixed = true;
    }

    if(type == mongo_database_type::NETWORK_PROPERTIES)
    {
        default_collection = "all_networks";
        is_fixed = true;
    }

    if(type == mongo_database_type::CHAT_MESSAGES)
    {
        default_collection = "all_chat_messages";
        is_fixed = true;
    }
}

void mongo_context::make_lock(const std::string& debug_info, const std::string& collection, size_t who)
{
    lock_internal* found = nullptr;

    {
        std::lock_guard guard(map_lock);

        found = &per_collection_lock[collection];
    }

    found->lock(debug_info, who);
}

void mongo_context::make_unlock(const std::string& collection)
{
    lock_internal* found = nullptr;

    {
        std::lock_guard guard(map_lock);

        found = &per_collection_lock[collection];
    }

    #ifdef DEADLOCK_DETECTION
    {
        std::lock_guard<lock_type_t> guard(thread_lock);

        thread_counter[std::this_thread::get_id()]--;
    }

    lg::log("Unlocking ", collection);
    #endif // DEADLOCK_DETECTION

    found->unlock();

    /*locked_by = -1;
    lock.unlock();*/
}

void database_read_interface::change_collection_unsafe(const std::string& coll, bool force_change)
{
    backend.change_collection_unsafe(coll, force_change);

    if(ctx->is_fixed && !force_change)
    {
        std::cout << "warning, collection should not be changed " << coll << " I am " << last_collection << std::endl;
        return;
    }

    if(coll == last_collection && !force_change)
        return;

    last_collection = coll;
}

void database_read_write_interface::insert_json_one_new(const nlohmann::json& json)
{
    backend.insert_one(json);
}

void database_read_write_interface::update_json_many_new(const nlohmann::json& selector, const nlohmann::json& update)
{
    backend.update_many(selector, update);
}

void database_read_write_interface::update_json_one_new(const nlohmann::json& selector, const nlohmann::json& update)
{
    backend.update_one(selector, update);
}

std::vector<nlohmann::json> database_read_interface::find_json_new(const nlohmann::json& json, const nlohmann::json& opts)
{
    return backend.find_many(json, opts);
}

void database_read_write_interface::remove_json_many_new(const nlohmann::json& json)
{
    backend.remove_many(json);
}

database_read_interface::database_read_interface(mongo_context* fctx) : backend(fctx)
{
    ctx = fctx;
}

database_read_write_interface::database_read_write_interface(mongo_context* fctx) : database_read_interface(fctx)
{

}

mongo_shim::mongo_shim(mongo_context* fctx, int plock_id)
{
    ctx = fctx;
    lock_id = plock_id;
}

tls_variable<int, -2> thread_id_storage_key;
tls_variable<int, 0> print_performance_diagnostics_key;
tls_variable<int, 0> should_throw;
tls_variable<int, 0> holds_lock;

int* tls_get_thread_id_storage_hack()
{
    return thread_id_storage_key.get();
}

int* tls_get_print_performance_diagnostics()
{
    return print_performance_diagnostics_key.get();
}

int* tls_get_should_throw()
{
    return should_throw.get();
}

int* tls_get_holds_lock()
{
    return holds_lock.get();
}

mongo_lock_proxy::mongo_lock_proxy(const mongo_shim& shim, bool lock) : ctx(shim.ctx)
{
    should_lock = lock;

    /*ctx = fctx;

    if(ctx == nullptr)
        return;*/

    /*size_t my_id = (size_t)&thread_id_storage_hack;
    static_assert(sizeof(my_id) == sizeof(&thread_id_storage_hack));*/

    ///ids don't need to be unique
    ///we just need to know what they are, and guarantee that in the command handler
    ///they aren't reused
    ///thread_id_storage_hack will default to 0
    ///except in the command handler we set this to be higher
    ///the *only* reason these ids exist is for external unlocking of locked resources in the context of
    ///uncooperative thread termination
    size_t my_id = *tls_get_thread_id_storage_hack();

    perf.enabled = (*tls_get_print_performance_diagnostics()) > 0;

    if(shim.ctx == nullptr)
        return;

    friendly_id = shim.lock_id;
    ilock_id = my_id;

    //if(ctx.ctx->default_collection != "")
    //    lock();

        //ctx.ctx->make_lock(fctx->last_db, fctx->default_collection, ilock_id, ctx.client);

    ctx.last_collection = ctx.ctx->default_collection;

    if(ctx.ctx->default_collection != "")
    {
        change_collection(ctx.ctx->default_collection, true);
    }
}

void mongo_lock_proxy::change_collection(const std::string& coll, bool force_change)
{
    ///need to alter locks
    unlock();

    ctx.change_collection_unsafe(coll, force_change);

    lock();
}

void mongo_lock_proxy::lock()
{
    if(!should_lock)
        return;

    if(!has_lock)
    {
        ctx.ctx->make_lock(ctx.ctx->last_db, ctx.last_collection, ilock_id);

        perf.locks++;

        if(perf.enabled)
            perf.lock_stacktraces.push_back(get_stacktrace());

        (*tls_get_holds_lock())++;
    }

    has_lock = true;
}

void mongo_lock_proxy::unlock()
{
    if(has_lock)
    {
        ctx.ctx->make_unlock(ctx.last_collection);

        (*tls_get_holds_lock())--;
    }

    has_lock = false;
}

mongo_lock_proxy::~mongo_lock_proxy()
{
    unlock();
}

mongo_nolock_proxy::mongo_nolock_proxy(const mongo_shim& shim) : mongo_lock_proxy(shim, false)
{

}

database_read_write_interface* mongo_lock_proxy::operator->()
{
    //lock();

    perf.db_hits++;

    return &ctx;
}

std::array<mongo_context*, (int)mongo_database_type::MONGO_COUNT> mongo_databases;

void initialse_db_all()
{
    for(int i=0; i < (int)mongo_database_type::MONGO_COUNT; i++)
        mongo_databases[i] = new mongo_context((mongo_database_type)i);

    atexit(cleanup_db_all);
}

void cleanup_db_all()
{
    ///first argument is irrelevant
    get_global_mongo_context(mongo_database_type::USER_ACCESSIBLE, true);
}
