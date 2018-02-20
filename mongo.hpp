#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <mongoc/mongoc.h>
#include <string>
#include <vector>
#include <iostream>
#include <mutex>

enum class mongo_database_type
{
    USER_ACCESSIBLE,
    USER_PROPERTIES,
    USER_ITEMS,
    GLOBAL_PROPERTIES,
    #if 0
    USER_AUTH,
    #endif // 0
};

std::string strip_whitespace(std::string);

struct mongo_context
{
    mongoc_client_t* client = nullptr;
    mongoc_database_t* database = nullptr;
    mongoc_collection_t* collection = nullptr;

    std::string last_collection = "";
    std::string last_db = "";

    static bool mongo_is_init;

    std::mutex lock;
    int locked_by = -1;

    ///need to run everything through a blacklist
    ///can probably just blacklist json

    ///if we ever have to add another db, make this fully data driven with structs and definitions and the like
    mongo_context(mongo_database_type type)
    {
        std::string uri_str = "Err";
        std::string db = "Err";

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

        #if 0
        if(type == mongo_database_type::USER_AUTH)
        {
            uri_str = "mongodb://user_auth_database:james20kuserhandlermongofunuserauth@localhost:27017/?authSource=users";
            db = "user_auth";
        }
        #endif // 0

        if(!mongo_is_init)
            mongoc_init();

        mongo_is_init = true;

        client = mongoc_client_new(uri_str.c_str());

        mongoc_client_set_appname(client, "crapmud");

        last_db = db;

        database = mongoc_client_get_database(client, db.c_str());

        if(type == mongo_database_type::USER_ITEMS)
        {
            change_collection("all_items");
        }

        if(type == mongo_database_type::GLOBAL_PROPERTIES)
        {
            change_collection("global_properties");
        }
    }

    bool contains_banned_query(bson_t* bs)
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

    void make_lock(int who)
    {
        lock.lock();

        locked_by = who;
    }

    void make_unlock()
    {
        locked_by = -1;

        lock.unlock();
    }

    void unlock_if(int who)
    {
        if(who == locked_by)
        {
            locked_by = -1;
            lock.unlock();
            printf("Salvaged db\n");
        }
    }

    void change_collection(const std::string& coll)
    {
        if(coll == last_collection)
            return;

        last_collection = coll;

        if(collection)
        {
            mongoc_collection_destroy(collection);
            collection = nullptr;
        }

        collection = mongoc_client_get_collection(client, last_db.c_str(), coll.c_str());
    }

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

    bson_t* make_bson_from_json(const std::string& json)
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

    void insert_bson_1(const std::string& script_host, bson_t* bs)
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

    void insert_json_1(const std::string& script_host, const std::string& json)
    {
        if(script_host != last_collection)
            return;

        bson_t* bs = make_bson_from_json(json);

        if(bs == nullptr)
            return;

        insert_bson_1(script_host, bs);

        bson_destroy(bs);
    }

    void update_bson_many(const std::string& script_host, bson_t* selector, bson_t* update)
    {
        if(selector == nullptr || update == nullptr)
            return;

        if(contains_banned_query(selector) || contains_banned_query(update))
        {
            printf("banned\n");
            return;
        }

        bson_error_t error;

        if(!mongoc_collection_update_many(collection, selector, update, nullptr, nullptr, &error))
        {
            fprintf (stderr, "err: %s\n", error.message);
        }
    }

    void update_json_many(const std::string& script_host, const std::string& selector, const std::string& update)
    {
        if(script_host != last_collection)
            return;

        bson_t* bs = make_bson_from_json(selector);
        bson_t* us = make_bson_from_json(update);

        update_bson_many(script_host, bs, us);

        bson_destroy(bs);
        bson_destroy(us);
    }

    #if 0
    std::vector<bson_t*> find_bson_raw(const std::string& script_host, bson_t* bs, bson_t* ps)
    {
        std::vector<bson_t*> results;

        if(script_host != last_collection)
            return results;

        if(bs == nullptr)
            return results;

        if(contains_banned_query(bs) || contains_banned_query(ps))
        {
            printf("banned\n");
            return {"Banned query"};
        }

        if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
            return results;

        while(mongoc_cursor_more(cursor) && mongoc_cursor_next (cursor, &doc))
        {


            results.push_back()

        }

        mongoc_cursor_destroy(cursor);
    }
    #endif // 0

    std::vector<std::string> find_bson(const std::string& script_host, bson_t* bs, bson_t* ps)
    {
        std::vector<std::string> results;

        if(script_host != last_collection)
            return results;

        if(bs == nullptr)
            return results;

        if(contains_banned_query(bs) || contains_banned_query(ps))
        {
            printf("banned\n");
            return {"Banned query"};
        }

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

        results = find_bson(script_host, bs, ps);

        if(ps)
            bson_destroy(ps);

        bson_destroy(bs);

        return results;
    }

    void remove_json(const std::string& script_host, const std::string& json)
    {
        if(script_host != last_collection)
            return;

        if(!mongoc_database_has_collection(database, last_collection.c_str(), nullptr))
        {
            return;
        }

        bson_t* bs = make_bson_from_json(json);

        mongoc_collection_delete_many(collection, bs, nullptr, nullptr, nullptr);

        bson_destroy(bs);
    }

    void insert_test_data()
    {
        bson_error_t error;

        bson_t* insert = BCON_NEW ("hello", BCON_UTF8 ("world"));

        if (!mongoc_collection_insert_one (collection, insert, NULL, NULL, &error))
        {
            fprintf (stderr, "%s\n", error.message);
        }

        bson_destroy (insert);
    }

    ~mongo_context()
    {
        if(collection)
            mongoc_collection_destroy (collection);

        mongoc_database_destroy (database);
        mongoc_client_destroy (client);

        if(mongo_is_init)
            mongoc_cleanup();

        mongo_is_init = false;
    }
};

struct mongo_lock_proxy
{
    mongo_context* ctx = nullptr;

    mongo_lock_proxy(mongo_context* fctx, int lock_id)
    {
        ctx = fctx;

        if(ctx == nullptr)
            return;

        ctx->make_lock(lock_id);
    }

    ~mongo_lock_proxy()
    {
        if(ctx == nullptr)
            return;

        ctx->make_unlock();
    }

    mongo_context* operator->() const
    {
        return ctx;
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

struct mongo_requester
{
    std::map<std::string, std::string> properties;
    std::map<std::string, int> is_binary;

    bool has_prop(const std::string& str)
    {
        return properties.find(str) != properties.end();
    }

    std::string get_prop(const std::string& str)
    {
        return properties[str];
    }

    int32_t get_prop_as_integer(const std::string& str)
    {
        std::string prop = properties[str];

        long long val = atoll(prop.c_str());

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

    std::vector<mongo_requester> fetch_from_db(mongo_lock_proxy& ctx)
    {
        std::vector<mongo_requester> ret;

        bson_t* to_find = bson_new();

        for(auto& i : properties)
        {
            if(is_binary[i.first])
                bson_append_binary(to_find, i.first.c_str(), i.first.size(), BSON_SUBTYPE_BINARY, (const uint8_t*)i.second.c_str(), i.second.size());
            else
                bson_append_utf8(to_find, i.first.c_str(), i.first.size(), i.second.c_str(), i.second.size());
        }

        std::vector<std::string> json_found = ctx->find_bson(ctx->last_collection, to_find, nullptr);

        for(auto& i : json_found)
        {
            mongo_requester found;

            bson_t* next = bson_new_from_json((const uint8_t*)i.c_str(), i.size(), nullptr);

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
            }

            bson_destroy(next);

            ret.push_back(found);
        }

        bson_destroy(to_find);

        return ret;
    }

    void insert_in_db(mongo_lock_proxy& ctx)
    {
        bson_t* to_insert = bson_new();

        for(auto& i : properties)
        {
            if(is_binary[i.first])
                bson_append_binary(to_insert, i.first.c_str(), i.first.size(), BSON_SUBTYPE_BINARY, (const uint8_t*)i.second.c_str(), i.second.size());
            else
                bson_append_utf8(to_insert, i.first.c_str(), i.first.size(), i.second.c_str(), i.second.size());
        }

        ctx->insert_bson_1(ctx->last_collection, to_insert);

        bson_destroy(to_insert);
    }
};

inline
void mongo_tests(const std::string& coll)
{
    ///mongoc_client_t *client = mongoc_client_new ("mongodb://user:password@localhost/?authSource=mydb");

    mongo_context ctx(mongo_database_type::USER_ACCESSIBLE);
    ctx.change_collection(coll);

    //ctx.insert_test_data();

    ctx.insert_json_1(coll, "{\"name\": {\"first\":\"bum\", \"last\":\"test\"}}");
}

#endif // MONGO_HPP_INCLUDED
