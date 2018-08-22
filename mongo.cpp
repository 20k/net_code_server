#include "mongo.hpp"
#include <json/json.hpp>

#include <chrono>
#include <set>
#include "logging.hpp"

#ifdef DEADLOCK_DETECTION
#include <boost/stacktrace.hpp>
#endif // DEADLOCK_DETECTION

#include <SFML/System.hpp>
#include <thread>
#include "stacktrace.hpp"

thread_local int mongo_lock_proxy::thread_id_storage_hack = -2;
thread_local int mongo_lock_proxy::print_performance_diagnostics = 0;

void lock_internal::lock(const std::string& debug_info, size_t who, mongoc_client_t* emergency)
{
    #ifdef DEADLOCK_DETECTION
    static std::atomic_int is_deadlocked{0};
    static std::mutex map_lock;
    static std::map<int, std::vector<std::string>> debug;
    static std::atomic_int crash_id{0};
    sf::Clock clk;
    #endif // DEADLOCK_DETECTION

    #ifdef SUPER_SAFE
    Sleep(1);
    #else
    std::this_thread::yield();
    #endif

    #define ATTEMPT_FASTER
    #ifdef ATTEMPT_FASTER
    //constexpr size_t max_retry = 500;
    //size_t cur_retry = 0;

    ///200 ms
    constexpr size_t max_microseconds_elapsed = 1000 * 200;
    bool sleeptime = false;

    sf::Clock clk;

    while(locked.test_and_set(std::memory_order_acquire))
    {
        if(sleeptime || clk.getElapsedTime().asMicroseconds() >= max_microseconds_elapsed)
        {
            sleeptime = true;
            Sleep(1);
        }
        else
        {
            std::this_thread::yield();
        }
    }

    #endif // ATTEMPT_FASTER

    #ifndef ATTEMPT_FASTER
    #ifndef DEADLOCK_DETECTION
    while(locked.test_and_set(std::memory_order_acquire))
    {
        Sleep(1);
    }
    #else

    {
        std::lock_guard guard(map_lock);
        debug[who].push_back(debug_info);
    }

    Sleep(1);

    while(locked.test_and_set(std::memory_order_acquire))
    {
        Sleep(1);

        if(clk.getElapsedTime().asSeconds() > 30 || (is_deadlocked == 1 && clk.getElapsedTime().asSeconds() > 5))
        {
            std::cout << "deadlock detected " << debug_info << " who: " + std::to_string(who) << " by " << locked_by << " debug " << debug_info << " with tid " << locked_by_tid << " my tid " << std::this_thread::get_id() << std::endl;
            lg::log("Deadlock ", debug_info, " who: ", std::to_string(who), " by ", locked_by, " debug ", debug_info);

            /*lg::log("Begin map info from ", who);

            {
                safe_lock_guard guard(map_lock);

                for(auto& i : debug)
                {
                    for(auto& k : i.second)
                        lg::log("me ", who, " them ", i.first, " coll ", k);
                }
            }*/

            lg::log("End map info from ", who);

            is_deadlocked = 1;

            //int my_crash_id = crash_id++;

            //std::cout << "call stack " << boost::stacktrace::stacktrace() << std::endl;

            //*lg::output << "crash with id " + std::to_string(my_crash_id) + " Start stacktrace: " << boost::stacktrace::stacktrace() << std::endl;

            Sleep(5000);
            //throw std::runtime_error("Deadlock " + std::to_string(who) + " " + debug_info);
        }
    }

    {
        std::lock_guard guard(map_lock);
        for(int i=0; i < (int)debug[who].size(); i++)
        {
            if(debug[who][i] == debug_info)
            {
                debug[who].erase(debug[who].begin() + i);
                break;
            }
        }
    }
    #endif // DEADLOCK_DETECTION
    #endif // 0

    locked_by = who;
    #ifdef DEADLOCK_DETECTION
    locked_by_tid = std::this_thread::get_id();
    locked_by_debug = debug_info;
    #endif // DEADLOCK_DETECTION
    in_case_of_emergency = emergency;
}

void lock_internal::unlock()
{
    #ifdef DEADLOCK_DETECTION
    locked_by_debug = "";
    #endif // DEADLOCK_DETECTION
    locked_by = 0;
    locked.clear(std::memory_order_release);
}


mongo_context::mongo_context(mongo_database_type type)
{
    std::string uri_str = "Err";
    std::string db = "Err";

    if(type == mongo_database_type::USER_ACCESSIBLE)
    {
        uri_str = "mongodb://user_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";;
        db = "user_dbs";

        client = mongoc_client_new(uri_str.c_str());
    }

    if(type == mongo_database_type::USER_PROPERTIES)
    {
        uri_str = "mongodb://user_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "user_properties";

        client = mongoc_client_new(uri_str.c_str());
    }

    if(type == mongo_database_type::USER_ITEMS)
    {
        uri_str = "mongodb://user_items_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "user_items";

        client = mongoc_client_new(uri_str.c_str());
    }

    if(type == mongo_database_type::GLOBAL_PROPERTIES)
    {
        uri_str = "mongodb://global_properties_database:james20kuserhandlermongofundiff@localhost:27017/?authSource=users";
        db = "global_properties";

        client = mongoc_client_new(uri_str.c_str());
    }

    /*if(type == mongo_database_type::CHAT_CHANNELS)
    {
        uri_str = "mongodb://chat_channels_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "chat_channels";
    }*/

    if(type == mongo_database_type::PENDING_NOTIFS)
    {
        uri_str = "mongodb://pending_notifs_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "pending_notifs";

        client = mongoc_client_new(uri_str.c_str());
    }

    if(type == mongo_database_type::CHAT_CHANNEL_PROPERTIES)
    {
        uri_str = "mongodb://chat_channel_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "chat_channel_properties";

        client = mongoc_client_new(uri_str.c_str());
    }

    if(type == mongo_database_type::NODE_PROPERTIES)
    {
        uri_str = "mongodb://node_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "node_properties";

        client = mongoc_client_new(uri_str.c_str());
    }

    if(type == mongo_database_type::NPC_PROPERTIES)
    {
        uri_str = "mongodb://npc_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "npc_properties";

        client = mongoc_client_new(uri_str.c_str());
    }

    ///hmm.. somewhat of a naming fuckup here
    ///TODO:
    ///When we full wipe and clean the db, normalise all names and passwords here
    if(type == mongo_database_type::NETWORK_PROPERTIES)
    {
        uri_str = "mongodb://network_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "all_networks";

        client = mongoc_client_new(uri_str.c_str());
    }

    std::map<mongo_database_type, std::string> procedural_dbs
    {
        {mongo_database_type::SCHEDULED_TASK, "SCHEDULED_TASK"},
        {mongo_database_type::LOW_LEVEL_STRUCTURE, "LOW_LEVEL_STRUCTURE"},
    };

    for(auto& i : procedural_dbs)
    {
        if(i.first == type)
        {
            uri_str = "mongodb://" + i.second + ":" + i.second + "handlermongofun@localhost:27017/?authSource=" + i.second;

            std::string turi = "mongodb://20k_admin:james20kcaterpillarmongofun@localhost:27017";

            db = i.second;
            default_collection = "all_" + i.second;
            is_fixed = true;

            std::cout << "curi " << uri_str << std::endl;

            mongoc_client_t* tclient = mongoc_client_new(turi.c_str());

            char** strv;
            bson_error_t error;

            if ((strv = mongoc_client_get_database_names_with_opts (tclient, NULL, &error)))
            {
                bool found = false;
                std::string name = i.second;

                for (int i = 0; strv[i]; i++)
                {
                    std::string str(strv[i]);

                    if(str == name)
                    {
                        found = true;
                        break;
                    }
                }

                if(!found)
                {
                    mongoc_database_t* ldb = mongoc_client_get_database(tclient, name.c_str());

                    nlohmann::json j;

                    std::vector<nlohmann::json> all_roles;

                    j["role"] = "readWrite";
                    j["db"] = i.second;

                    all_roles.push_back(j);

                    nlohmann::json fin;
                    fin = all_roles;

                    //nlohmann::json fin;
                    //fin["roles"] = all_roles;

                    std::string str = fin.dump();

                    std::cout << "json " << str << std::endl;

                    bson_t* bson = bson_new_from_json((const uint8_t*)str.c_str(), str.size(), nullptr);

                    mongoc_database_add_user(ldb, name.c_str(), (name + "handlermongofun").c_str(), bson, nullptr, nullptr);

                    mongoc_database_create_collection(ldb, default_collection.c_str(), nullptr, nullptr);
                    //mongoc_collection_t* col = mongoc_database_create_collection(ldb, default_collection.c_str(), nullptr, nullptr);
                    //mongoc_collection_destroy(col);

                    mongoc_database_destroy(ldb);

                    bson_destroy(bson);
                }

                bson_strfreev (strv);
            }
            else
            {
                fprintf (stderr, "Command failed: %s\n", error.message);
            }

            mongoc_client_destroy(tclient);

            client = mongoc_client_new(uri_str.c_str());
        }
    }

    mongoc_client_set_appname(client, "crapmud");

    uri = mongoc_uri_new(uri_str.c_str());
    pool = mongoc_client_pool_new(uri);
    mongoc_client_pool_set_error_api(pool, 2);

    #if 0
    if(type == mongo_database_type::USER_AUTH)
    {
        uri_str = "mongodb://user_auth_database:james20kuserhandlermongofunuserauth@localhost:27017/?authSource=users";
        db = "user_auth";
    }
    #endif // 0

    last_db = db;

    //database = mongoc_client_get_database(client, db.c_str());

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

    /*if(type == mongo_database_type::CHAT_CHANNELS)
    {
        change_collection("all_channels");
    }*/

    /*if(type == mongo_database_type::PENDING_NOTIFS)
    {
        change_collection("all_notifs");
    }*/

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
}

void mongo_context::map_lock_for()
{
    ///8 second lock
    int time_ms = 8 * 1000;

    while(!map_lock.try_lock_for(std::chrono::milliseconds(time_ms))){}
}

void mongo_context::make_lock(const std::string& debug_info, const std::string& collection, size_t who, mongoc_client_t* in_case_of_emergency)
{
    map_lock_for();

    auto& found = per_collection_lock[collection];

    map_lock.unlock();

    #ifdef DEADLOCK_DETECTION
    {
        std::lock_guard<std::mutex> guard(thread_lock);

        thread_counter[std::this_thread::get_id()]++;

        if(thread_counter[std::this_thread::get_id()] > 1)
        {
            printf("WOW BAD\n");
        }
    }

    lg::log("Locking ", debug_info, " ", collection, " ", std::to_string(who));
    #endif // DEADLOCK_DETECTION

    //std::cout << "lock collection " << collection << " on " << this << std::endl;

    //found.lock();

    found.lock(debug_info, who, in_case_of_emergency);

    //lg::log("Locked ", debug_info, " " , std::to_string(who));

    /*lock.lock();

    locked_by = who;*/
}

void mongo_context::make_unlock(const std::string& collection)
{
    map_lock_for();

    auto& found = per_collection_lock[collection];

    map_lock.unlock();

    #ifdef DEADLOCK_DETECTION
    {
        std::lock_guard<std::mutex> guard(thread_lock);

        thread_counter[std::this_thread::get_id()]--;
    }

    lg::log("Unlocking ", collection);
    #endif // DEADLOCK_DETECTION

    found.unlock();

    /*locked_by = -1;
    lock.unlock();*/
}

void mongo_context::unlock_if(size_t who)
{
    //if(who == locked_by)
    {
        /*safe_lock_guard lck(internal_safety);

        map_lock.lock();

        per_collection_lock[last_collection];
        auto found = per_collection_lock.find(last_collection);

        map_lock.unlock();

        if(found->second.locked_by == who)
            found->second.unlock();

        printf("Salvaged db\n");*/

        map_lock_for();

        for(auto& i : per_collection_lock)
        {
            if(i.second.locked_by == who)
            {
                return_client(i.second.in_case_of_emergency);
                i.second.unlock();
                printf("salvaged db\n");
            }
        }

        map_lock.unlock();
    }
}

#if 0
void ping()
{
    bson_t* command = BCON_NEW ("ping", BCON_INT32 (1));

    bson_t reply;

    bson_error_t error;

    bool retval = mongoc_client_command_simple (
                      client, "admin", command, NULL, &reply, &error);

    if (!retval)
    {
        fprintf (stderr, "%s\n", error.message);

        return;
    }

    char* str = bson_as_json (&reply, NULL);
    printf ("%s\n", str);

    bson_destroy (&reply);
    bson_destroy (command);
    bson_free (str);
}
#endif // 0

#if 0
void insert_test_data() const
{
    bson_error_t error;

    bson_t* insert = BCON_NEW ("hello", BCON_UTF8 ("world"));

    if (!mongoc_collection_insert_one (collection, insert, NULL, NULL, &error))
    {
        fprintf (stderr, "%s\n", error.message);
    }

    bson_destroy (insert);
}
#endif

mongoc_client_t* mongo_context::request_client()
{
    return mongoc_client_pool_pop(pool);
}

void mongo_context::return_client(mongoc_client_t* pclient)
{
    return mongoc_client_pool_push(pool, pclient);
}

mongo_context::~mongo_context()
{
    //if(collection)
    //    mongoc_collection_destroy (collection);

    //mongoc_database_destroy (database);
    mongoc_client_destroy (client);
    mongoc_uri_destroy (uri);
}


bool mongo_interface::contains_banned_query(bson_t* bs) const
{
    if(bs == nullptr)
        return false;

    std::vector<std::string> banned
    {
        "$where",
        "$expr",
        "$maxTimeMS",
        "$query",
        "$showDiskLoc"
    };

    bson_iter_t iter;

    if(bson_iter_init(&iter, bs))
    {
        while(bson_iter_next(&iter))
        {
            std::string key = bson_iter_key(&iter);

            for(auto& i : banned)
            {
                if(strip_whitespace(key) == i)
                    return true;
            }
        }
    }

    return false;
}

void mongo_interface::change_collection_unsafe(const std::string& coll, bool force_change)
{
    if(ctx->is_fixed && !force_change)
    {
        std::cout << "warning, collection should not be changed" << std::endl;
        return;
    }

    if(coll == last_collection && !force_change)
        return;

    last_collection = coll;

    if(collection)
    {
        mongoc_collection_destroy(collection);
        collection = nullptr;
    }

    collection = mongoc_client_get_collection(client, ctx->last_db.c_str(), coll.c_str());
}

bson_t* mongo_interface::make_bson_from_json(const std::string& json) const
{
    bson_error_t error;

    bson_t* bson = bson_new_from_json ((const uint8_t *)json.c_str(), -1, &error);

    if (!bson)
    {
        //std::cout << "errd " << json << std::endl;

        //fprintf (stderr, "bson err: %s\n", error.message);
        return nullptr;
    }

    return bson;
}

bson_t* mongo_interface::make_bson_from_json_err(const std::string& json, std::string& err) const
{
    bson_error_t error;

    bson_t* bson = bson_new_from_json ((const uint8_t *)json.c_str(), -1, &error);

    if (!bson)
    {
        //std::cout << "errd " << json << std::endl;

        //fprintf (stderr, "bson err: %s\n", error.message);
        err = error.message;
        return nullptr;
    }

    err = "";
    return bson;
}

void mongo_interface::insert_bson_1(const std::string& script_host, bson_t* bs) const
{
    if(script_host != last_collection)
        return;

    if(contains_banned_query(bs))
    {
        printf("banned\n");
        return;
    }

    bson_error_t error;

    if(!mongoc_collection_insert_one(collection, bs, NULL, NULL, &error))
    {
        printf("Error: %s\n", error.message);
        //fprintf (stderr, "err: %s\n", error.message);
    }
}

void mongo_interface::insert_json_1(const std::string& script_host, const std::string& json) const
{
    if(script_host != last_collection)
        return;

    bson_t* bs = make_bson_from_json(json);

    if(bs == nullptr)
        return;

    insert_bson_1(script_host, bs);

    bson_destroy(bs);
}

std::string mongo_interface::update_bson_many(const std::string& script_host, bson_t* selector, bson_t* update) const
{
    if(selector == nullptr || update == nullptr)
        return "Null pointer";

    if(contains_banned_query(selector) || contains_banned_query(update))
    {
        //printf("banned\n");
        return "Contains banned query";
    }

    bson_error_t error;

    if(!mongoc_collection_update_many(collection, selector, update, nullptr, nullptr, &error))
    {
        fprintf (stderr, "err: %s\n", error.message);

        return error.message;
    }

    return "";
}

std::string mongo_interface::update_json_many(const std::string& script_host, const std::string& selector, const std::string& update) const
{
    if(script_host != last_collection)
        return "Wrong collection, this is an internal error";

    std::string err;

    bson_t* bs = make_bson_from_json_err(selector, err);

    if(bs == nullptr)
        return err;

    bson_t* us = make_bson_from_json_err(update, err);

    if(us == nullptr)
    {
        bson_destroy(bs);
        return err;
    }

    std::string update_err = update_bson_many(script_host, bs, us);

    bson_destroy(bs);
    bson_destroy(us);

    return update_err;
}

std::string mongo_interface::update_bson_one(bson_t* selector, bson_t* update) const
{
    if(selector == nullptr || update == nullptr)
        return "Null pointer";

    if(contains_banned_query(selector) || contains_banned_query(update))
    {
        //printf("banned\n");
        return "Contains banned query";
    }

    bson_error_t error;

    if(!mongoc_collection_update_one(collection, selector, update, nullptr, nullptr, &error))
    {
        fprintf (stderr, "err: %s\n", error.message);

        return error.message;
    }

    return "";
}


std::string mongo_interface::update_json_one(const std::string& selector, const std::string& update) const
{
    std::string err;

    bson_t* bs = make_bson_from_json_err(selector, err);

    if(bs == nullptr)
        return err;

    bson_t* us = make_bson_from_json_err(update, err);

    if(us == nullptr)
    {
        bson_destroy(bs);
        return err;
    }

    std::string update_err = update_bson_one(bs, us);

    bson_destroy(bs);
    bson_destroy(us);

    return update_err;
}


/*bool has_collection(const std::string& coll)
{
    return !mongoc_database_has_collection()
}*/

std::vector<std::string> mongo_interface::find_bson(const std::string& script_host, bson_t* bs, bson_t* ps)
{
    std::vector<std::string> results;

    if(bs == nullptr)
        return results;

    if(contains_banned_query(bs) || contains_banned_query(ps))
    {
        //printf("banned\n");
        return {"Banned query"};
    }

    if(script_host != last_collection)
        return results;

    if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
    {
        return std::vector<std::string>();
    }

    const bson_t *doc;

    ///hmm. for .first() we should limit to one doc
    ///for .count we need to run a completely separate query
    ///for array, we need to do everythang
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, bs, ps, nullptr);

    while(mongoc_cursor_more(cursor) && mongoc_cursor_next (cursor, &doc))
    {
        char* str = bson_as_json(doc, NULL);

        if(str == nullptr)
            continue;

        results.push_back(str);

        bson_free(str);
    }

    mongoc_cursor_destroy(cursor);

    return results;
}

std::vector<std::string> mongo_interface::find_json(const std::string& script_host, const std::string& json, const std::string& proj)
{
    std::vector<std::string> results;

    if(script_host != last_collection)
        return results;

    //printf("find\n");

    bson_t* bs = make_bson_from_json(json);
    bson_t* ps = make_bson_from_json(proj);

    if(bs == nullptr)
        return results;

    //std::cout << " bs " << bs << " ps " << ps << std::endl;

    results = find_bson(script_host, bs, ps);

    //std::cout <<" found\n";

    if(ps)
        bson_destroy(ps);

    //std::cout << "f1\n";

    bson_destroy(bs);

    //std::cout << "f2\n";

    return results;
}

void mongo_interface::remove_bson(const std::string& script_host, bson_t* bs)
{
    if(script_host != last_collection)
        return;

    if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
        return;

    if(bs == nullptr)
        return;

    mongoc_collection_delete_many(collection, bs, nullptr, nullptr, nullptr);
}

void mongo_interface::remove_json(const std::string& script_host, const std::string& json)
{
    if(script_host != last_collection)
        return;

    if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
        return;

    bson_t* bs = make_bson_from_json(json);

    if(bs == nullptr)
        return;

    remove_bson(script_host, bs);

    bson_destroy(bs);
}

mongo_interface::mongo_interface(mongo_context* fctx)
{
    ctx = fctx;
    client = fctx->request_client();

    database = mongoc_client_get_database(client, ctx->last_db.c_str());
}

/*mongo_interface::mongo_interface(mongo_interface&& other)
{
    client = other.client;
    ctx = other.ctx;
    database = other.database;
    collection = other.collection;
    last_collection = other.last_collection;
    moved_from = other.moved_from;

    other.moved_from = true;
}*/

mongo_interface::~mongo_interface()
{
    ctx->return_client(client);

    if(collection)
        mongoc_collection_destroy (collection);

    mongoc_database_destroy (database);
}

mongo_shim::mongo_shim(mongo_context* fctx, int plock_id)
{
    ctx = fctx;
    lock_id = plock_id;
}

mongo_lock_proxy::mongo_lock_proxy(const mongo_shim& shim) : ctx(shim.ctx)
{
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
    size_t my_id = thread_id_storage_hack;

    perf.enabled = print_performance_diagnostics > 0;

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
    if(!has_lock)
    {
        ctx.ctx->make_lock(ctx.ctx->last_db, ctx.last_collection, ilock_id, ctx.client);

        perf.locks++;

        if(perf.enabled)
            perf.lock_stacktraces.push_back(get_stacktrace());
    }

    has_lock = true;
}

void mongo_lock_proxy::unlock()
{
    if(has_lock)
    {
        ctx.ctx->make_unlock(ctx.last_collection);
    }

    has_lock = false;
}

mongo_lock_proxy::~mongo_lock_proxy()
{
    unlock();
}

mongo_nolock_proxy::mongo_nolock_proxy(const mongo_shim& shim) : mongo_lock_proxy(shim)
{
    /*size_t my_id = thread_id_storage_hack;

    if(fctx == nullptr)
        return;

    friendly_id = shim.lock_id;
    ilock_id = my_id;

    ctx.last_collection = fctx->default_collection;

    if(ctx.ctx->default_collection != "")
    {
        change_collection(ctx.ctx->default_collection, true);
    }*/
}

mongo_interface* mongo_lock_low_level::operator->()
{
    //lock();

    return &ctx;
}

std::vector<mongo_requester> mongo_requester::fetch_from_db(mongo_lock_proxy& ctx)
{
    std::vector<mongo_requester> ret;

    bson_t* to_find = bson_new();

    append_properties_all_to(to_find);

    for(auto& i : exists_check)
    {
        if(!i.second)
            continue;

        bson_t child;

        bson_append_document_begin(to_find, i.first.c_str(), strlen(i.first.c_str()), &child);

        BSON_APPEND_INT32(&child, "$exists", 1);

        bson_append_document_end(to_find, &child);
    }

    //if(lt_than.size() != 0 && gt_than.size() != 0)
    {
        bson_t child;

        std::set<std::string> keys_check;

        for(auto& i : lt_than)
            keys_check.insert(i.first);

        for(auto& i : gt_than)
            keys_check.insert(i.first);

        for(auto& i : lt_than_i)
            keys_check.insert(i.first);

        for(auto& i : gt_than_i)
            keys_check.insert(i.first);

        for(auto& i : keys_check)
        {
            std::string key = i;

            bool has_lt_than = lt_than.find(key) != lt_than.end();
            bool has_gt_than = gt_than.find(key) != gt_than.end();

            std::string lt_val = has_lt_than ? lt_than[key] : "";
            std::string gt_val = has_gt_than ? gt_than[key] : "";

            bson_append_document_begin(to_find, key.c_str(), key.size(), &child);

            if(gt_val != "")
                BSON_APPEND_UTF8(&child, "$gt", gt_val.c_str());

            if(lt_val != "")
                BSON_APPEND_UTF8(&child, "$lt", lt_val.c_str());

            if(lt_than_i.find(key) != lt_than_i.end())
                BSON_APPEND_INT32(&child, "$lt", lt_than_i[key]);

            if(gt_than_i.find(key) != gt_than_i.end())
                BSON_APPEND_INT32(&child, "$gt", gt_than_i[key]);

            //std::cout << "$gt " << gt_val << " $lt " << lt_val << std::endl;

            bson_append_document_end(to_find, &child);
        }
    }


    bson_t* to_opt = nullptr;

    if(limit >= 0)
    {
        if(to_opt == nullptr)
            to_opt = bson_new();

        BSON_APPEND_INT64(to_opt, "limit", limit);
    }

    if(sort_on.size() != 0)
    {
        if(to_opt == nullptr)
            to_opt = bson_new();

        bson_t child;

        bson_append_document_begin(to_opt, "sort", 4, &child);

        for(auto& i : sort_on)
        {
            BSON_APPEND_INT64(&child, i.first.c_str(), i.second);
        }

        bson_append_document_end(to_opt, &child);
    }

    std::vector<std::string> json_found = ctx->find_bson(ctx->last_collection, to_find, to_opt);

    for(auto& i : json_found)
    {
        mongo_requester found;

        bson_t* next = bson_new_from_json((const uint8_t*)i.c_str(), i.size(), nullptr);

        if(next == nullptr)
        {
            printf("invalid json in find\n");
            continue;
        }

        bson_iter_t iter;
        bson_iter_init(&iter, next);

        while (bson_iter_next (&iter))
        {
            std::string key = bson_iter_key(&iter);

            if(BSON_ITER_HOLDS_BINARY(&iter))
            {
                found.set_prop_bin(key, bson_iter_binary_std_string(&iter));
                continue;
            }

            if(BSON_ITER_HOLDS_UTF8(&iter))
            {
                found.set_prop(key, bson_iter_utf8_easy(&iter));
                continue;
            }

            if(BSON_ITER_HOLDS_INT32(&iter))
            {
                found.set_prop_int(key, bson_iter_int32(&iter));
                continue;
            }

            if(BSON_ITER_HOLDS_DOUBLE(&iter))
            {
                found.set_prop_double(key, bson_iter_double(&iter));
                continue;
            }

            ///if we'd done this right, this would compose
            ///sadly it is not done right
            if(BSON_ITER_HOLDS_ARRAY(&iter))
            {
                std::vector<std::string> arr;

                bson_iter_t child;

                bson_iter_recurse(&iter, &child);

                while(bson_iter_next(&child))
                {
                    if(BSON_ITER_HOLDS_UTF8(&child))
                    {
                        arr.push_back(bson_iter_utf8_easy(&child));
                    }
                }

                found.set_prop_array(key, arr);
                continue;
            }
        }

        bson_destroy(next);

        ret.push_back(found);
    }

    if(to_opt != nullptr)
        bson_destroy(to_opt);

    bson_destroy(to_find);

    return ret;
}

void mongo_requester::insert_in_db(mongo_lock_proxy& ctx)
{
    bson_t* to_insert = bson_new();

    append_properties_all_to(to_insert);

    ctx->insert_bson_1(ctx->last_collection, to_insert);

    bson_destroy(to_insert);
}

void mongo_requester::append_property_to(bson_t* bson, const std::string& key)
{
    std::string val = properties[key];

    if(is_binary[key])
        bson_append_binary(bson, key.c_str(), key.size(), BSON_SUBTYPE_BINARY, (const uint8_t*)val.c_str(), val.size());
    else if(is_integer[key])
        BSON_APPEND_INT32(bson, key.c_str(), get_prop_as_integer(key));
    else if(is_double[key])
        BSON_APPEND_DOUBLE(bson, key.c_str(), get_prop_as_double(key));
    else if(is_arr[key])
    {
        bson_t child;
        bson_append_array_begin(bson, key.c_str(), key.size(), &child);

        for(int i=0; i < (int)arr_props[key].size(); i++)
        {
            std::string arr_key = std::to_string(i);

            bson_append_utf8(&child, arr_key.c_str(), arr_key.size(), arr_props[key][i].c_str(), arr_props[key][i].size());
        }

        bson_append_array_end(bson, &child);
    }
    else
        bson_append_utf8(bson, key.c_str(), key.size(), val.c_str(), val.size());
}

void mongo_requester::append_properties_all_to(bson_t* bson)
{
    for(auto& i : properties)
    {
        append_property_to(bson, i.first);
    }

    for(auto& i : arr_props)
    {
        append_property_to(bson, i.first);
    }
}

/*void update_in_db_if_exists(mongo_lock_proxy& ctx, mongo_requester& set_to)
{
    bson_t* to_select = bson_new();

    bson_t child;

    for(auto& i : properties)
    {
        bson_append_document_begin(to_select, i.first.c_str(), i.first.size(), &child);

        BSON_APPEND_BOOL(&child, "$exists", true);

        bson_append_document_end(to_select, &child);
    }

    for(auto& i : arr_props)
    {
        bson_append_document_begin(to_select, i.first.c_str(), i.first.size(), &child);

        BSON_APPEND_BOOL(&child, "$exists", true);

        bson_append_document_end(to_select, &child);
    }

    bson_t* to_update = bson_new();

    BSON_APPEND_DOCUMENT_BEGIN(to_update, "$set", &child);

    set_to.append_properties_all_to(&child);

    bson_append_document_end(to_update, &child);

    //std::cout << "JSON " << bson_as_json(to_select, nullptr) << " selector " << bson_as_json(to_update, nullptr) << std::endl;

    ctx->update_bson_many(ctx->last_collection, to_select, to_update);

    bson_destroy(to_update);
    bson_destroy(to_select);
}*/

void mongo_requester::update_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to)
{
    bson_t* to_select = bson_new();

    append_properties_all_to(to_select);

    bson_t* to_update = bson_new();

    bson_t child;

    BSON_APPEND_DOCUMENT_BEGIN(to_update, "$set", &child);

    set_to.append_properties_all_to(&child);

    bson_append_document_end(to_update, &child);

    ctx->update_bson_many(ctx->last_collection, to_select, to_update);

    bson_destroy(to_update);
    bson_destroy(to_select);
}

void mongo_requester::update_one_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to)
{
    bson_t* to_select = bson_new();

    append_properties_all_to(to_select);

    bson_t* to_update = bson_new();

    bson_t child;

    BSON_APPEND_DOCUMENT_BEGIN(to_update, "$set", &child);

    set_to.append_properties_all_to(&child);

    bson_append_document_end(to_update, &child);

    ctx->update_bson_one(to_select, to_update);

    bson_destroy(to_update);
    bson_destroy(to_select);
}

void mongo_requester::remove_all_from_db(mongo_lock_proxy& ctx)
{
    bson_t* to_remove = bson_new();

    append_properties_all_to(to_remove);

    ctx->remove_bson(ctx->last_collection, to_remove);

    bson_destroy(to_remove);
}
