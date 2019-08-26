#include "mongo.hpp"
#include <nlohmann/json.hpp>

#include <chrono>
#include <set>
#include "logging.hpp"

#ifdef DEADLOCK_DETECTION
#include <boost/stacktrace.hpp>
#endif // DEADLOCK_DETECTION

#include <SFML/System.hpp>
#include <thread>
#include "stacktrace.hpp"
#include "safe_thread.hpp"
#include <libncclient/nc_util.hpp>

#include "rate_limiting.hpp"
#include "tls.hpp"

//#define ONLY_VALIDATION

#ifndef USE_MONGO
#undef ONLY_VALIDATION
#endif // USE_MONGO

//thread_local int mongo_lock_proxy::thread_id_storage_hack = -2;
//thread_local int mongo_lock_proxy::print_performance_diagnostics = 0;

/*nlohmann::json bson_to_json(bson_t* bs)
{
    if(bs == nullptr)
    {
        return nlohmann::json();
    }

    size_t len = 0;
    char* str = bson_as_relaxed_extended_json(bs, &len);

    std::string ret(str, len);

    return nlohmann::json::parse(ret);
}

std::string bson_iter_binary_std_string(bson_iter_t* iter)
{
    uint32_t len = 0;
    const uint8_t* binary = nullptr;
    bson_subtype_t subtype = BSON_SUBTYPE_BINARY;

    bson_iter_binary(iter, &subtype, &len, &binary);

    if(binary == nullptr)
    {
        printf("warning invalid bson_iter_binary_std_string\n");
        return std::string();
    }

    std::string value((const char*)binary, len);

    return value;
}

std::string bson_iter_utf8_easy(bson_iter_t* iter)
{
    uint32_t len = bson_iter_utf8_len_unsafe(iter);
    const char* k = bson_iter_utf8(iter, &len);

    if(k == nullptr)
    {
        printf("warning invalid bson_iter_utf8_easy\n");

        return std::string();
    }

    return std::string(k, len);
}*/

#ifdef USE_MONGO
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
    sthread::this_yield();
    #endif

    #define ATTEMPT_FASTER
    #ifdef ATTEMPT_FASTER
    //constexpr size_t max_retry = 500;
    //size_t cur_retry = 0;

    ///200 ms
    constexpr size_t max_microseconds_elapsed = 1000 * 20;
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
            sthread::this_yield();
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
#else
void lock_internal::lock(const std::string& debug_info, size_t who)
{
    #ifndef USE_STD_MUTEX
    sthread::this_yield();

    ///200 ms
    constexpr size_t max_microseconds_elapsed = 1000 * 20;
    bool sleeptime = false;

    sf::Clock clk;

    size_t cycles = 1;

    while(locked.test_and_set(std::memory_order_acquire))
    {
        /*if(sleeptime || cycles > 1000)
        {
            COOPERATE_KILL_THREAD_LOCAL_URGENT();

            sleeptime = true;
            Sleep(1);
        }
        else
        {
            sthread::this_yield();
            cycles++;
        }*/

        if((cycles & (128 - 1)) == 0)
            sthread::this_yield();

        if((cycles & (1024 - 1)) == 0)
        {
            sthread::this_sleep(1);
            COOPERATE_KILL_THREAD_LOCAL_URGENT();
        }

        cycles++;
    }
    #else
    mut_lock.lock();
    #endif // USE_STD_MUTEX

    locked_by = who;
}
#endif

void lock_internal::unlock()
{
    #ifdef DEADLOCK_DETECTION
    locked_by_debug = "";
    #endif // DEADLOCK_DETECTION
    locked_by = 0;

    #ifndef USE_STD_MUTEX
    locked.clear(std::memory_order_release);
    #else
    mut_lock.unlock();
    #endif
}


mongo_context::mongo_context(mongo_database_type type)
{
    std::string uri_str = "";
    std::string db = "Err";

    last_db_type = type;

    if(type == mongo_database_type::USER_ACCESSIBLE)
    {
        uri_str = "mongodb://user_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";;
        db = "user_dbs";
    }

    if(type == mongo_database_type::USER_PROPERTIES)
    {
        uri_str = "mongodb://user_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "user_properties";
    }

    if(type == mongo_database_type::USER_ITEMS)
    {
        uri_str = "mongodb://user_items_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "user_items";
    }

    if(type == mongo_database_type::GLOBAL_PROPERTIES)
    {
        uri_str = "mongodb://global_properties_database:james20kuserhandlermongofundiff@localhost:27017/?authSource=users";
        db = "global_properties";
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
    }

    if(type == mongo_database_type::CHAT_CHANNEL_PROPERTIES)
    {
        uri_str = "mongodb://chat_channel_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "chat_channel_properties";
    }

    if(type == mongo_database_type::NODE_PROPERTIES)
    {
        uri_str = "mongodb://node_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "node_properties";
    }

    if(type == mongo_database_type::NPC_PROPERTIES)
    {
        uri_str = "mongodb://npc_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "npc_properties";
    }

    ///hmm.. somewhat of a naming fuckup here
    ///TODO:
    ///When we full wipe and clean the db, normalise all names and passwords here
    if(type == mongo_database_type::NETWORK_PROPERTIES)
    {
        uri_str = "mongodb://network_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        db = "all_networks";
    }

    if(uri_str != "")
    {
        #ifdef USE_MONGO
        client = mongoc_client_new(uri_str.c_str());
        #endif // USE_MONGO
    }

    std::map<mongo_database_type, std::string> procedural_dbs
    {
        {mongo_database_type::SCHEDULED_TASK, "SCHEDULED_TASK"},
        {mongo_database_type::LOW_LEVEL_STRUCTURE, "LOW_LEVEL_STRUCTURE"},
        {mongo_database_type::QUEST_MANAGER, "QUEST_MANAGER"},
        {mongo_database_type::EVENT_MANAGER, "EVENT_MANAGER"},
        {mongo_database_type::MEMORY_CORE, "MEMORY_CORE"},
    };

    std::map<mongo_database_type, bool> is_fixed_map
    {
        {mongo_database_type::SCHEDULED_TASK, true},
        {mongo_database_type::LOW_LEVEL_STRUCTURE, true},
        {mongo_database_type::QUEST_MANAGER, true},
        {mongo_database_type::EVENT_MANAGER, false},
        {mongo_database_type::MEMORY_CORE, true},
    };

    for(auto& i : procedural_dbs)
    {
        if(i.first == type)
        {
            uri_str = "mongodb://" + i.second + ":" + i.second + "handlermongofun@localhost:27017/?authSource=" + i.second;

            std::string turi = "mongodb://20k_admin:james20kcaterpillarmongofun@localhost:27017/?authSource=admin";

            db = i.second;

            if(is_fixed_map[i.first])
            {
                default_collection = "all_" + i.second;
                is_fixed = true;

                std::cout << "curi " << uri_str << std::endl;
            }
            else
            {
                #ifdef USE_MONGO
                static_assert(false, "Not implemented in mongodb");
                #endif // USE_MONGO

                continue;
            }

            #ifdef USE_MONGO
            #ifndef TESTING
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


                    bson_error_t add_error;
                    bool add_success = mongoc_database_add_user(ldb, name.c_str(), (name + "handlermongofun").c_str(), bson, nullptr, &add_error);

                    if(!add_success)
                    {
                        std::cout << "failed to add user to db " << add_error.message << std::endl;
                    }

                    mongoc_client_t* aclient = mongoc_client_new(uri_str.c_str());
                    mongoc_database_t* new_db = mongoc_client_get_database(aclient, name.c_str());

                    bson_error_t err;

                    auto coll = mongoc_database_create_collection(new_db, default_collection.c_str(), nullptr, &err);

                    if(coll != nullptr)
                        mongoc_collection_destroy(coll);
                    else
                        std::cout << "Error creating database of default coll " << default_collection << " " << err.message << std::endl;

                    mongoc_database_destroy(new_db);
                    mongoc_client_destroy(aclient);

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

            #endif // TESTING

            client = mongoc_client_new(uri_str.c_str());
            #endif // USE_MONGO
        }
    }

    #ifdef USE_MONGO
    mongoc_client_set_appname(client, "crapmud");

    uri = mongoc_uri_new(uri_str.c_str());
    pool = mongoc_client_pool_new(uri);
    mongoc_client_pool_set_error_api(pool, 2);
    #endif // USE_MONGO

    #if 0
    if(type == mongo_database_type::USER_AUTH)
    {
        uri_str = "mongodb://user_auth_database:james20kuserhandlermongofunuserauth@localhost:27017/?authSource=users";
        db = "user_auth";
    }
    #endif // 0

    last_db = db;

    #ifdef USE_MONGO
    database = mongoc_client_get_database(client, db.c_str());
    #endif // USE_MONGO

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

    #ifdef USE_MONGO
    char** strv;

    if((strv = mongoc_database_get_collection_names_with_opts(database, nullptr, nullptr)))
    {
        for(int i = 0; strv[i]; i++)
        {
            std::string str(strv[i]);

            all_collections.push_back(str);
        }

        bson_strfreev (strv);
    }
    else
    {
        assert(false);
    }

    mongoc_database_destroy(database);
    database = nullptr;
    #endif // USE_MONGO
}

void mongo_context::map_lock_for()
{
    ///8 second lock
    int time_ms = 8 * 1000;

    while(!map_lock.try_lock_for(std::chrono::milliseconds(time_ms))){}
}

#ifdef USE_MONGO
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
#else
void mongo_context::make_lock(const std::string& debug_info, const std::string& collection, size_t who)
{
    map_lock_for();

    auto& found = per_collection_lock[collection];

    map_lock.unlock();

    found.lock(debug_info, who);
}
#endif

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
                #ifdef USE_MONGO
                return_client(i.second.in_case_of_emergency);
                #endif // USE_MONGO
                i.second.unlock();
                printf("salvaged db\n");
            }
        }

        map_lock.unlock();
    }
}

#ifdef USE_MONGO
mongoc_client_t* mongo_context::request_client()
{
    return mongoc_client_pool_pop(pool);
}

void mongo_context::return_client(mongoc_client_t* pclient)
{
    return mongoc_client_pool_push(pool, pclient);
}
#endif // USE_MONGO

mongo_context::~mongo_context()
{
    #ifdef USE_MONGO
    //if(collection)
    //    mongoc_collection_destroy (collection);

    //mongoc_database_destroy (database);
    mongoc_client_destroy (client);
    mongoc_uri_destroy (uri);
    #endif // USE_MONGO
}


/*bool mongo_interface::contains_banned_query(bson_t* bs) const
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
}*/

void mongo_interface::change_collection_unsafe(const std::string& coll, bool force_change)
{
    if(enable_testing_backend)
        backend.change_collection_unsafe(coll, force_change);

    if(ctx->is_fixed && !force_change)
    {
        std::cout << "warning, collection should not be changed" << std::endl;
        return;
    }

    if(coll == last_collection && !force_change)
        return;

    last_collection = coll;

    #ifdef USE_MONGO
    if(collection)
    {
        mongoc_collection_destroy(collection);
        collection = nullptr;
    }

    collection = mongoc_client_get_collection(client, ctx->last_db.c_str(), coll.c_str());
    #endif // USE_MONGO
}

/*bson_t* mongo_interface::make_bson_from_json(const std::string& json) const
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

//#define ONLY_VALIDATION

void mongo_interface::insert_bson_1(const std::string& script_host, bson_t* bs)
{
    if(script_host != last_collection)
        return;

    if(contains_banned_query(bs))
    {
        printf("banned\n");
        return;
    }

    if(enable_testing_backend)
        backend.insert_one(bson_to_json(bs));
    #ifndef ONLY_VALIDATION
    else
    #endif // ONLY_VALIDATION
    {
        #ifdef USE_MONGO
        bson_error_t error;

        if(!mongoc_collection_insert_one(collection, bs, NULL, NULL, &error))
        {
            printf("Error: %s\n", error.message);
            //fprintf (stderr, "err: %s\n", error.message);
        }
        #endif // USE_MONGO
    }
}

void mongo_interface::insert_json_1(const std::string& script_host, const std::string& json)
{
    if(script_host != last_collection)
        return;

    bson_t* bs = make_bson_from_json(json);

    if(bs == nullptr)
        return;

    insert_bson_1(script_host, bs);

    bson_destroy(bs);
}*/


void mongo_interface::insert_json_one_new(const nlohmann::json& json)
{
    if(enable_testing_backend)
    {
        backend.insert_one(json);
    }
    #ifndef ONLY_VALIDATION
    else
    #endif // ONLY_VALIDATION
    {
        throw std::runtime_error("Unimplemented mongo");
        //insert_json_1(last_collection, json.dump());
    }
}

std::string mongo_interface::update_json_many_new(const nlohmann::json& selector, const nlohmann::json& update)
{
    std::string res;

    if(enable_testing_backend)
    {
        backend.update_many(selector, update);
    }
    #ifndef ONLY_VALIDATION
    else
    #endif // ONLY_VALIDATION
    {
        throw std::runtime_error("Unimplemented mongo");
        //res = update_json_many(last_collection, selector.dump(), update.dump());
    }

    return res;
}

std::string mongo_interface::update_json_one_new(const nlohmann::json& selector, const nlohmann::json& update)
{
    std::string res;

    if(enable_testing_backend)
    {
        backend.update_one(selector, update);
    }
    #ifndef ONLY_VALIDATION
    else
    #endif // ONLY_VALIDATION
    {
        throw std::runtime_error("Unimplemented mongo");
        //res = update_json_one(selector.dump(), update.dump());
    }

    return res;
}

/*std::string mongo_interface::update_bson_many(const std::string& script_host, bson_t* selector, bson_t* update)
{
    if(selector == nullptr || update == nullptr)
        return "Null pointer";

    if(contains_banned_query(selector) || contains_banned_query(update))
    {
        //printf("banned\n");
        return "Contains banned query";
    }

    if(enable_testing_backend)
        backend.update_many(bson_to_json(selector), bson_to_json(update));

    #ifndef ONLY_VALIDATION
    else
    {
    #endif // ONLY_VALIDATION
        #ifdef USE_MONGO
        bson_error_t error;

        if(!mongoc_collection_update_many(collection, selector, update, nullptr, nullptr, &error))
        {
            fprintf (stderr, "err: %s\n", error.message);

            return error.message;
        }
        #endif // USE_MONGO
    #ifndef ONLY_VALIDATION
    }
    #endif // ONLY_VALIDATION

    return "";
}*/

/*std::string mongo_interface::update_json_many(const std::string& script_host, const std::string& selector, const std::string& update)
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
}*/

/*std::string mongo_interface::update_bson_one(bson_t* selector, bson_t* update)
{
    if(selector == nullptr || update == nullptr)
        return "Null pointer";

    if(contains_banned_query(selector) || contains_banned_query(update))
    {
        //printf("banned\n");
        return "Contains banned query";
    }

    if(enable_testing_backend)
        backend.update_one(bson_to_json(selector), bson_to_json(update));

    #ifndef ONLY_VALIDATION
    else
    {
    #endif // ONLY_VALIDATION
    #ifdef USE_MONGO
        bson_error_t error;

        if(!mongoc_collection_update_one(collection, selector, update, nullptr, nullptr, &error))
        {
            fprintf (stderr, "err: %s\n", error.message);

            return error.message;
        }
    #endif // USE_MONGO
    #ifndef ONLY_VALIDATION
    }
    #endif // ONLY_VALIDATION

    return "";
}


std::string mongo_interface::update_json_one(const std::string& selector, const std::string& update)
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
}*/


/*bool has_collection(const std::string& coll)
{
    return !mongoc_database_has_collection()
}*/

#if 0
///https://jira.mongodb.org/browse/SERVER-4462
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

    #ifdef USE_MONGO
    if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
    {
        return std::vector<std::string>();
    }
    #endif // USE_MONGO

    #ifndef ONLY_VALIDATION
    if(!enable_testing_backend)
    {
    #endif // ONLY_VALIDATION
    #ifdef USE_MONGO
        const bson_t *doc;

        ///hmm. for .first() we should limit to one doc
        ///for .count we need to run a completely separate query
        ///for array, we need to do everythang
        mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, bs, ps, nullptr);

        int skipped = 0;

        while(mongoc_cursor_next (cursor, &doc))
        {
            char* str = bson_as_relaxed_extended_json(doc, NULL);

            if(str == nullptr)
            {
                skipped++;
                continue;
            }

            //#ifdef ONLY_VALIDATION
            results.push_back(str);
            //#endif // ONLY_VALIDATION

            bson_free(str);
        }

        mongoc_cursor_destroy(cursor);
    #endif // USE_MONGO
    #ifndef ONLY_VALIDATION
    } else
    #else
    if(enable_testing_backend)
    #endif
    {
        std::vector<nlohmann::json> validated = backend.find_many(bson_to_json(bs), bson_to_json(ps));

        #ifdef ONLY_VALIDATION
        if(validated.size() != results.size())
        {
            //std::cout << "back " << get_stacktrace() << std::endl;

            std::cout << "invalid validated size " << validated.size() << " " << results.size() << std::endl;
            std::cout << "bs " << bson_to_json(bs).dump() + " ps " + bson_to_json(ps).dump() << std::endl;

            std::cout << "failed validation from ctx " << ctx->last_db << std::endl;

            if(results.size() > 0)
            {
                std::cout << results[0] << std::endl;
            }

            for(auto& i : validated)
            {
                std::cout << "validated " << i << std::endl;
            }

            std::cout << "has skipped " << skipped << std::endl;
        }
        else
        {
            std::vector<nlohmann::json> res_copy;

            for(int i=0; i < (int)validated.size(); i++)
            {
                auto parsed = nlohmann::json::parse(results[i]);

                res_copy.push_back(parsed);
            }

            for(int i=0; i < (int)validated.size(); i++)
            {
                remove_mongo_id(res_copy[i]);
                remove_mongo_id(validated[i]);
            }

            std::sort(validated.begin(), validated.end());
            std::sort(res_copy.begin(), res_copy.end());

            for(int i=0; i < (int)validated.size(); i++)
            {
                remove_mongo_id(res_copy[i]);
                remove_mongo_id(validated[i]);

                if(validated[i] != res_copy[i])
                {
                    std::cout << "bad find, json " << validated[i] << " real db " << res_copy[i] << std::endl;
                    std::cout << "request bs " << bson_to_json(bs).dump() + " ps " + bson_to_json(ps).dump() << std::endl;

                    std::cout << "failed validation from ctx " << ctx->last_db << std::endl;
                }
            }
        }
        #else
        for(auto& i : validated)
        {
            results.push_back(i.dump());
        }
        #endif // ONLY_VALIDATION
    }

    return results;
}
#endif // 0

#if 0
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
#endif // 0

std::vector<nlohmann::json> mongo_interface::find_json_new(const nlohmann::json& json, const nlohmann::json& opts)
{
    #ifndef ONLY_VALIDATION
    if(!enable_testing_backend)
    #endif // ONLY_VALIDATION
    {
        /*std::vector<std::string> found = find_json(last_collection, json.dump(), opts.dump());

        std::vector<nlohmann::json> ret;

        for(auto& i : found)
        {
            ret.push_back(nlohmann::json::parse(i));
        }

        return ret;*/

        throw std::runtime_error("Unimplemented mongo");
    }
    #ifndef ONLY_VALIDATION
    else
    #endif // ONLY_VALIDATION
    {
        return backend.find_many(json, opts);
    }
}

/*void mongo_interface::remove_bson(const std::string& script_host, bson_t* bs)
{
    if(script_host != last_collection)
        return;

    #ifdef USE_MONGO
    if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
        return;
    #endif // USE_MONGO

    if(bs == nullptr)
        return;

    if(enable_testing_backend)
        backend.remove_many(bson_to_json(bs));

    #ifdef USE_MONGO
    #ifndef ONLY_VALIDATION
    else
    #endif // ONLY_VALIDATION
        mongoc_collection_delete_many(collection, bs, nullptr, nullptr, nullptr);
    #endif // USE_MONGO
}

void mongo_interface::remove_json(const std::string& script_host, const std::string& json)
{
    if(script_host != last_collection)
        return;

    #ifdef USE_MONGO
    if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
        return;
    #endif // USE_MONGO

    bson_t* bs = make_bson_from_json(json);

    if(bs == nullptr)
        return;

    remove_bson(script_host, bs);

    bson_destroy(bs);
}*/

void mongo_interface::remove_json_many_new(const nlohmann::json& json)
{
    #ifndef ONLY_VALIDATION
    if(!enable_testing_backend)
    #endif // ONLY_VALIDATION
    {
        #ifdef USE_MONGO
        if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
            return;

        bson_t* bs = make_bson_from_json(json.dump());

        if(bs == nullptr)
            return;

        remove_bson(last_collection, bs);

        bson_destroy(bs);
        #endif // USE_MONGO
    }
    #ifndef ONLY_VALIDATION
    else
    #endif // ONLY_VALIDATION
    if(enable_testing_backend)
    {
        backend.remove_many(json);
    }
}

mongo_interface::mongo_interface(mongo_context* fctx) : backend(fctx)
{
    ctx = fctx;

    #ifdef USE_MONGO
    client = fctx->request_client();

    database = mongoc_client_get_database(client, ctx->last_db.c_str());
    #endif // USE_MONGO
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
    #ifdef USE_MONGO
    ctx->return_client(client);

    if(collection)
        mongoc_collection_destroy (collection);

    mongoc_database_destroy (database);
    #endif // USE_MONGO
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
        #ifdef USE_MONGO
        ctx.ctx->make_lock(ctx.ctx->last_db, ctx.last_collection, ilock_id, ctx.client);
        #else
        ctx.ctx->make_lock(ctx.ctx->last_db, ctx.last_collection, ilock_id);
        #endif

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

mongo_interface* mongo_lock_low_level::operator->()
{
    //lock();

    perf.db_hits++;

    return &ctx;
}

std::vector<mongo_requester> mongo_requester::fetch_from_db(mongo_lock_proxy& ctx)
{
    nlohmann::json json_properties = get_all_properties_json();

    for(auto& i : exists_check)
    {
        if(!i.second)
            continue;

        nlohmann::json exist;
        exist["$exists"] = 1;

        json_properties[i.first] = exist;
    }

    nlohmann::json json_opt;

    if(sort_on.size() != 0)
    {
        json_opt["sort"] = sort_on;
    }

    std::vector<nlohmann::json> json_found_from_json = ctx->find_json_new(json_properties, json_opt);

    std::vector<mongo_requester> alt_method;

    for(const nlohmann::json& obj : json_found_from_json)
    {
        mongo_requester found;

        for(auto& pairs : obj.get<nlohmann::json::object_t>())
        {
            const std::string& key = pairs.first;
            const nlohmann::json& val = pairs.second;

            if(val.is_number())
            {
                found.set_prop_double(key, (double)val);
                continue;
            }

            if(val.is_array())
            {
                std::vector<std::string> data;

                for(auto& arr_mem : val)
                {
                    if(!arr_mem.is_string())
                        continue;

                    data.push_back(arr_mem);
                }

                found.set_prop_array(key, data);
                continue;
            }

            if(val.is_string())
            {
                found.set_prop(key, val.get<std::string>());
                continue;
            }
        }

        alt_method.push_back(found);
    }

    return alt_method;
}

void mongo_requester::insert_in_db(mongo_lock_proxy& ctx)
{
    auto all_props = get_all_properties_json();

    ctx->insert_json_one_new(all_props);
}

/*void mongo_requester::append_property_to(bson_t* bson, const std::string& key)
{
    std::string val = properties[key];

    //if(is_binary[key])
    //    bson_append_binary(bson, key.c_str(), key.size(), BSON_SUBTYPE_BINARY, (const uint8_t*)val.c_str(), val.size());
    if(is_integer[key])
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

        //std::cout << bson_to_json(&child) << std::endl;

        bson_append_array_end(bson, &child);

        //std::cout << bson_to_json(bson) << std::endl;
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
}*/

void mongo_requester::append_property_json(nlohmann::json& js, const std::string& key)
{
    if(is_integer[key])
        js[key] = get_prop_as_integer(key);
    else if(is_double[key])
        js[key] = get_prop_as_double(key);
    else if(is_arr[key])
        js[key] = arr_props[key];
    else
        js[key] = properties[key];
}

nlohmann::json mongo_requester::get_all_properties_json()
{
    nlohmann::json js;

    for(auto& i : properties)
    {
        append_property_json(js, i.first);
    }

    for(auto& i : arr_props)
    {
        append_property_json(js, i.first);
    }

    return js;
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

///replace these with new version
///creates {"$set" : {obj:1, obj2:1}} etc i think
void mongo_requester::update_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to)
{
    nlohmann::json all_props = get_all_properties_json();
    nlohmann::json all_props_new = set_to.get_all_properties_json();

    nlohmann::json setter;
    setter["$set"] = all_props_new;

    ctx->update_json_many_new(all_props, setter);
}

void mongo_requester::update_one_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to)
{
    nlohmann::json all_props = get_all_properties_json();
    nlohmann::json all_props_new = set_to.get_all_properties_json();

    nlohmann::json setter;
    setter["$set"] = all_props_new;

    ctx->update_json_one_new(all_props, setter);
}

void mongo_requester::remove_all_from_db(mongo_lock_proxy& ctx)
{
    nlohmann::json props = get_all_properties_json();

    ctx->remove_json_many_new(props);
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

/*bson_t* make_bson_default()
{
    bson_t* bs = new bson_t;
    bson_init(bs);

    return bs;
}

void destroy_bson_default(bson_t* t)
{
    if(t == nullptr)
        return;

    bson_destroy(t);

    delete t;
}*/
