#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <mongoc/mongoc.h>
#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include <set>
#include <map>
#include <atomic>
#include <chrono>
#include "logging.hpp"
#include <boost/stacktrace.hpp>

//#define DEADLOCK_DETECTION
#ifdef DEADLOCK_DETECTION
#include <SFML/System.hpp>
#endif // DEADLOCK_DETECTION
#include <thread>

enum class mongo_database_type
{
    USER_ACCESSIBLE,
    USER_PROPERTIES,
    USER_ITEMS,
    GLOBAL_PROPERTIES,
    #if 0
    USER_AUTH,
    #endif // 0
    //CHAT_CHANNELS, ///deprecated
    PENDING_NOTIFS,
    CHAT_CHANNEL_PROPERTIES,
    NODE_PROPERTIES,
    NPC_PROPERTIES,
    NETWORK_PROPERTIES,
    SCHEDULED_TASK,
    MONGO_COUNT
};

std::string strip_whitespace(std::string);

struct lock_internal
{
    int locked_by = -1;
    #ifdef DEADLOCK_DETECTION
    std::thread::id locked_by_tid;
    std::string locked_by_debug;
    #endif // DEADLOCK_DETECTION
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
    mongoc_client_t* in_case_of_emergency = nullptr;

    void lock(const std::string& debug_info, int who, mongoc_client_t* emergency)
    {
        #ifdef DEADLOCK_DETECTION
        static std::atomic_int is_deadlocked{0};
        static std::mutex map_lock;
        static std::map<int, std::vector<std::string>> debug;
        static std::atomic_int crash_id{0};
        sf::Clock clk;
        #endif // DEADLOCK_DETECTION

        Sleep(1);

        #ifndef DEADLOCK_DETECTION
        while(locked.test_and_set(std::memory_order_acquire)){Sleep(1);}
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

        locked_by = who;
        #ifdef DEADLOCK_DETECTION
        locked_by_tid = std::this_thread::get_id();
        locked_by_debug = debug_info;
        #endif // DEADLOCK_DETECTION
        in_case_of_emergency = emergency;
    }

    void unlock()
    {
        #ifdef DEADLOCK_DETECTION
        locked_by_debug = "";
        #endif // DEADLOCK_DETECTION
        locked_by = -1;
        locked.clear(std::memory_order_release);
    }
};

///hmm
///maybe move most of mongo context into lock proxy
///and get the client in a thread safe way
struct mongo_context
{
    mongoc_uri_t* uri = nullptr;
    mongoc_client_pool_t* pool = nullptr;

    mongoc_client_t* client = nullptr;
    mongoc_database_t* database = nullptr;
    //mongoc_collection_t* collection = nullptr;

    //std::string last_collection = "";
    std::string last_db = "";

    std::string default_collection = "";

    ///thread safety of below map
    ///make timed
    std::recursive_timed_mutex map_lock;
    //std::mutex lock;
    //int locked_by = -1;

    ///this isn't for thread safety, this is for marshalling db access
    std::map<std::string, lock_internal> per_collection_lock;

    static inline std::map<std::thread::id, std::atomic_int> thread_counter;
    static inline std::mutex thread_lock;

    //lock_internal found;

    //static std::mutex found;

    bool is_fixed = false;

    ///need to run everything through a blacklist
    ///can probably just blacklist json

    ///if we ever have to add another db, make this fully data driven with structs and definitions and the like
    mongo_context(mongo_database_type type);

    void map_lock_for()
    {
        ///8 second lock
        int time_ms = 8 * 1000;

        //map_lock.lock();

        while(!map_lock.try_lock_for(std::chrono::milliseconds(time_ms))){}
    }

    void make_lock(const std::string& debug_info, const std::string& collection, int who, mongoc_client_t* in_case_of_emergency)
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

    void make_unlock(const std::string& collection)
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

    void unlock_if(int who)
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

    mongoc_client_t* request_client()
    {
        return mongoc_client_pool_pop(pool);
    }

    void return_client(mongoc_client_t* pclient)
    {
        return mongoc_client_pool_push(pool, pclient);
    }

    ~mongo_context()
    {
        //if(collection)
        //    mongoc_collection_destroy (collection);

        //mongoc_database_destroy (database);
        mongoc_client_destroy (client);
        mongoc_uri_destroy (uri);
    }
};

template<typename T>
struct safe_lock_guard
{
    std::lock_guard<T> guard;

    safe_lock_guard(T& t) : guard(t)
    {
        #ifdef DEADLOCK_DETECTION
        std::lock_guard<std::mutex> g(mongo_context::thread_lock);

        mongo_context::thread_counter[std::this_thread::get_id()]++;

        if(mongo_context::thread_counter[std::this_thread::get_id()] > 1)
        {
            printf("bad guard\n");

            std::cout << boost::stacktrace::stacktrace() << std::endl;
        }
        #endif // DEADLOCK_DETECTION
    }

    ~safe_lock_guard()
    {
        #ifdef DEADLOCK_DETECTION
        std::lock_guard<std::mutex> g(mongo_context::thread_lock);

        mongo_context::thread_counter[std::this_thread::get_id()]--;
        #endif // DEADLOCK_DETECTION
    }
};

struct mongo_interface
{
    mongoc_client_t* client = nullptr;
    mongo_context* ctx = nullptr;

    mongoc_database_t* database = nullptr;
    mongoc_collection_t* collection = nullptr;

    std::string last_collection;

    bool contains_banned_query(bson_t* bs) const
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

    void change_collection_unsafe(const std::string& coll, bool force_change = false)
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

    bson_t* make_bson_from_json(const std::string& json) const
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

    bson_t* make_bson_from_json_err(const std::string& json, std::string& err) const
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

    void insert_bson_1(const std::string& script_host, bson_t* bs) const
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
            fprintf (stderr, "err: %s\n", error.message);
        }
    }

    void insert_json_1(const std::string& script_host, const std::string& json) const
    {
        if(script_host != last_collection)
            return;

        bson_t* bs = make_bson_from_json(json);

        if(bs == nullptr)
            return;

        insert_bson_1(script_host, bs);

        bson_destroy(bs);
    }

    std::string update_bson_many(const std::string& script_host, bson_t* selector, bson_t* update) const
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

    std::string update_json_many(const std::string& script_host, const std::string& selector, const std::string& update) const
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

    /*bool has_collection(const std::string& coll)
    {
        return !mongoc_database_has_collection()
    }*/

    std::vector<std::string> find_bson(const std::string& script_host, bson_t* bs, bson_t* ps)
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

    std::vector<std::string> find_json(const std::string& script_host, const std::string& json, const std::string& proj)
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

    void remove_bson(const std::string& script_host, bson_t* bs)
    {
        if(script_host != last_collection)
            return;

        if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
            return;

        if(bs == nullptr)
            return;

        mongoc_collection_delete_many(collection, bs, nullptr, nullptr, nullptr);
    }

    void remove_json(const std::string& script_host, const std::string& json)
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

    mongo_interface(mongo_context* fctx)
    {
        ctx = fctx;
        client = fctx->request_client();

        database = mongoc_client_get_database(client, ctx->last_db.c_str());
    }

    ~mongo_interface()
    {
        ctx->return_client(client);

        if(collection)
            mongoc_collection_destroy (collection);

        mongoc_database_destroy (database);
    }
};

struct mongo_lock_proxy
{
    mongo_interface ctx;

    int ilock_id = -1;

    mongo_lock_proxy(mongo_context* fctx, int lock_id) : ctx(fctx)
    {
        /*ctx = fctx;

        if(ctx == nullptr)
            return;*/

        if(fctx == nullptr)
            return;

        if(fctx->default_collection != "")
            ctx.ctx->make_lock(fctx->last_db, fctx->default_collection, lock_id, ctx.client);

        ilock_id = lock_id;
        ctx.last_collection = fctx->default_collection;

        if(ctx.ctx->default_collection != "")
        {
            change_collection(ctx.ctx->default_collection, true);
        }
    }

    mongo_lock_proxy(const mongo_lock_proxy&) = delete;

    void change_collection(const std::string& coll, bool force_change = false)
    {
        ///need to alter locks
        if(ctx.last_collection != "")
            ctx.ctx->make_unlock(ctx.last_collection);

        ctx.change_collection_unsafe(coll, force_change);
        ctx.ctx->make_lock(ctx.ctx->last_db, coll, ilock_id, ctx.client);
    }

    ~mongo_lock_proxy()
    {
        ctx.ctx->make_unlock(ctx.last_collection);
    }

    mongo_interface* operator->()
    {
        return &ctx;
    }
};

#include "mongo_cleanup.hpp"

//https://stackoverflow.com/questions/30166706/c-convert-simple-values-to-string
template<typename T>
inline
typename std::enable_if<std::is_fundamental<T>::value, std::string>::type stringify_hack(const T& t)
{
    return std::to_string(t);
}

template<typename T>
inline
typename std::enable_if<!std::is_fundamental<T>::value, std::string>::type  stringify_hack(const T& t)
{
    return std::string(t);
}

inline
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

inline
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
}

///ok, support for arrays is now non negotiable
struct mongo_requester
{
    std::map<std::string, std::string> properties;
    std::map<std::string, int> is_binary;
    std::map<std::string, int> is_integer;
    std::map<std::string, int> is_double;
    std::map<std::string, int> is_arr;

    std::map<std::string, std::vector<std::string>> arr_props;

    std::map<std::string, int> sort_on;

    std::map<std::string, int> exists_check;
    std::map<std::string, std::string> gt_than;
    std::map<std::string, std::string> lt_than;

    std::map<std::string, int32_t> gt_than_i;
    std::map<std::string, int32_t> lt_than_i;

    int64_t limit = -1;

    bool has_prop(const std::string& str) const
    {
        return (properties.find(str) != properties.end()) || (arr_props.find(str) != arr_props.end());
    }

    std::string get_prop(const std::string& str) const
    {
        if(!has_prop(str))
            return std::string();

        return properties.at(str);
    }

    std::vector<std::string> get_prop_as_array(const std::string& str)
    {
        if(!has_prop(str))
            return std::vector<std::string>();

        return arr_props[str];
    }

    int64_t get_prop_as_integer(const std::string& str) const
    {
        if(!has_prop(str))
            return int64_t();

        std::string prop = properties.at(str);

        if(prop == "")
            return 0;

        long long val = atoll(prop.c_str());

        return val;
    }

    double get_prop_as_double(const std::string& str)
    {
        if(!has_prop(str))
            return double();

        std::string prop = properties[str];

        if(prop == "")
            return 0;

        auto val = atof(prop.c_str());

        return val;
    }

    template<typename T>
    void set_prop(const std::string& key, const T& value)
    {
        properties[key] = stringify_hack(value);
    }

    template<typename T>
    void set_prop_bin(const std::string& key, const T& value)
    {
        properties[key] = stringify_hack(value);
        is_binary[key] = 1;
    }

    template<typename T>
    void set_prop_int(const std::string& key, const T& value)
    {
        properties[key] = stringify_hack(value);
        is_integer[key] = 1;
    }

    template<typename T>
    void set_prop_double(const std::string& key, const T& value)
    {
        properties[key] = stringify_hack(value);
        is_double[key] = 1;
    }

    void set_prop_sort_on(const std::string& key, int dir)
    {
        sort_on[key] = dir;
    }

    void set_prop_array(const std::string& key, const std::vector<std::string>& vals)
    {
        arr_props[key] = vals;
        is_arr[key] = 1;
    }

    template<typename T>
    void set_prop_array(const std::string& key, const std::vector<T>& vals)
    {
        std::vector<std::string> strs;

        for(auto& i : vals)
        {
            strs.push_back(stringify_hack(i));
        }

        arr_props[key] = strs;
        is_arr[key] = 1;
    }

    void set_limit(int64_t limit_)
    {
        limit = limit_;
    }

    std::vector<mongo_requester> fetch_from_db(mongo_lock_proxy& ctx)
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

    void insert_in_db(mongo_lock_proxy& ctx)
    {
        bson_t* to_insert = bson_new();

        append_properties_all_to(to_insert);

        ctx->insert_bson_1(ctx->last_collection, to_insert);

        bson_destroy(to_insert);
    }

    void append_property_to(bson_t* bson, const std::string& key)
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

    void append_properties_all_to(bson_t* bson)
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

    void update_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to)
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

    void remove_all_from_db(mongo_lock_proxy& ctx)
    {
        bson_t* to_remove = bson_new();

        append_properties_all_to(to_remove);

        ctx->remove_bson(ctx->last_collection, to_remove);

        bson_destroy(to_remove);
    }
};

inline
void mongo_tests(const std::string& coll)
{
    ///mongoc_client_t *client = mongoc_client_new ("mongodb://user:password@localhost/?authSource=mydb");

    #if 0
    mongo_context ctx(mongo_database_type::USER_ACCESSIBLE);
    ctx.change_collection_unsafe(coll);

    //ctx.insert_test_data();

    ctx.insert_json_1(coll, "{\"name\": {\"first\":\"bum\", \"last\":\"test\"}}");
    #endif // 0
}

#endif // MONGO_HPP_INCLUDED
