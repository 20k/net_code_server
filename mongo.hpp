#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <mongoc/mongoc.h>
#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include <map>
#include <atomic>
#include <thread>
#include "perfmon.hpp"

#include <json/json.hpp>

//#define DEADLOCK_DETECTION

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
    LOW_LEVEL_STRUCTURE,
    MONGO_COUNT
};

std::string strip_whitespace(std::string);

struct lock_internal
{
    size_t locked_by = -1;
    #ifdef DEADLOCK_DETECTION
    std::thread::id locked_by_tid;
    std::string locked_by_debug;
    #endif // DEADLOCK_DETECTION
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
    mongoc_client_t* in_case_of_emergency = nullptr;

    void lock(const std::string& debug_info, size_t who, mongoc_client_t* emergency);
    void unlock();
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

    std::string last_db = "";

    std::string default_collection = "";

    ///thread safety of below map
    ///make timed
    std::recursive_timed_mutex map_lock;

    ///this isn't for thread safety, this is for marshalling db access
    std::map<std::string, lock_internal> per_collection_lock;

    static inline std::map<std::thread::id, std::atomic_int> thread_counter;
    static inline std::mutex thread_lock;

    bool is_fixed = false;

    ///need to run everything through a blacklist
    ///can probably just blacklist json

    ///if we ever have to add another db, make this fully data driven with structs and definitions and the like
    mongo_context(mongo_database_type type);

    void map_lock_for();

    void make_lock(const std::string& debug_info, const std::string& collection, size_t who, mongoc_client_t* in_case_of_emergency);

    void make_unlock(const std::string& collection);

    void unlock_if(size_t who);

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

    mongoc_client_t* request_client();

    void return_client(mongoc_client_t* pclient);

    ~mongo_context();
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

    bool contains_banned_query(bson_t* bs) const;

    void change_collection_unsafe(const std::string& coll, bool force_change = false);

    bson_t* make_bson_from_json(const std::string& json) const;
    bson_t* make_bson_from_json_err(const std::string& json, std::string& err) const;

    void insert_bson_1(const std::string& script_host, bson_t* bs) const;
    void insert_json_1(const std::string& script_host, const std::string& json) const;

    std::string update_bson_many(const std::string& script_host, bson_t* selector, bson_t* update) const;
    std::string update_json_many(const std::string& script_host, const std::string& selector, const std::string& update) const;

    std::string update_bson_one(bson_t* selector, bson_t* update) const;
    std::string update_json_one(const std::string& selector, const std::string& update) const;

    /*bool has_collection(const std::string& coll)
    {
        return !mongoc_database_has_collection()
    }*/

    std::vector<std::string> find_bson(const std::string& script_host, bson_t* bs, bson_t* ps);
    std::vector<std::string> find_json(const std::string& script_host, const std::string& json, const std::string& proj);

    void remove_bson(const std::string& script_host, bson_t* bs);
    void remove_json(const std::string& script_host, const std::string& json);

    //mongo_interface(mongo_interface&&);
    mongo_interface(mongo_context* fctx);
    ~mongo_interface();
};

struct mongo_shim
{
    mongo_context* ctx = nullptr;
    int lock_id = -1;

    mongo_shim(mongo_context* fctx, int lock_id);
};

struct mongo_lock_proxy
{
    static thread_local int thread_id_storage_hack;
    static thread_local int print_performance_diagnostics;

    mongo_interface ctx;

    size_t ilock_id = 0;
    int friendly_id = -1;

    bool has_lock = false;
    bool should_lock = true;

    perfmon perf;

    mongo_lock_proxy(const mongo_shim& shim, bool lock = true);
    mongo_lock_proxy(const mongo_lock_proxy&) = delete;

    void change_collection(const std::string& coll, bool force_change = false);

    virtual void lock();
    void unlock();

    virtual ~mongo_lock_proxy();

    mongo_interface* operator->();
};

struct mongo_nolock_proxy : mongo_lock_proxy
{
    mongo_nolock_proxy(const mongo_shim& shim);
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

    std::vector<mongo_requester> fetch_from_db(mongo_lock_proxy& ctx);

    void insert_in_db(mongo_lock_proxy& ctx);

    void append_property_to(bson_t* bson, const std::string& key);

    void append_properties_all_to(bson_t* bson);

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

    void update_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to);
    void update_one_in_db_if_exact(mongo_lock_proxy& ctx, mongo_requester& set_to);

    void remove_all_from_db(mongo_lock_proxy& ctx);
};

inline
std::vector<nlohmann::json> fetch_from_db(mongo_lock_proxy& ctx, nlohmann::json fnd, nlohmann::json proj = {})
{
    std::vector<nlohmann::json> ret;

    auto found = ctx->find_json(ctx->last_collection, fnd.dump(), proj.dump());

    for(auto& i : found)
    {
        ret.push_back(nlohmann::json::parse(i));
    }

    return ret;
}

inline
void remove_all_from_db(mongo_lock_proxy& ctx, nlohmann::json rem)
{
    ctx->remove_json(ctx->last_collection, rem.dump());
}

inline
void update_in_db_if_exact(mongo_lock_proxy& ctx, nlohmann::json to_select, nlohmann::json to_update)
{
    nlohmann::json to_set;

    to_set["$set"] = to_update;

    //std::cout << "TO SET " << to_set.dump() << std::endl;
    //std::cout << "TO select " << to_select.dump() << std::endl;

    ctx->update_json_many(ctx->last_collection, to_select.dump(), to_set.dump());
}

inline
void insert_in_db(mongo_lock_proxy& ctx, nlohmann::json to_insert)
{
    ctx->insert_json_1(ctx->last_collection, to_insert.dump());
}

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
