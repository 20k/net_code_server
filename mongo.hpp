#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <mongoc/mongoc.h>
#include <string>
#include <vector>
#include <iostream>

enum class mongo_database_type
{
    USER_ACCESSIBLE,
    USER_PROPERTIES,
    USER_ITEMS,
};

struct mongo_context
{
    mongoc_client_t* client = nullptr;
    mongoc_database_t* database = nullptr;
    mongoc_collection_t* collection = nullptr;

    std::string last_collection = "";
    std::string last_db = "";

    static bool mongo_is_init;

    ///need to run everything through a blacklist
    ///can probably just blacklist json

    mongo_context(mongo_database_type type)
    {
        std::string uri_str_accessible = "mongodb://user_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        std::string uri_str_properties = "mongodb://user_properties_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";
        std::string uri_str_items      = "mongodb://user_items_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";

        std::string uri_str = "Err";
        std::string db = "Err";


        if(type == mongo_database_type::USER_ACCESSIBLE)
        {
            uri_str = uri_str_accessible;
            db = "user_dbs";
        }

        if(type == mongo_database_type::USER_PROPERTIES)
        {
            uri_str = uri_str_properties;
            db = "user_properties";
        }

        if(type == mongo_database_type::USER_ITEMS)
        {
            uri_str = uri_str_items;
            db = "user_items";
        }

        if(!mongo_is_init)
            mongoc_init();

        mongo_is_init = true;

        client = mongoc_client_new(uri_str.c_str());

        mongoc_client_set_appname(client, "crapmud");

        last_db = db;

        database = mongoc_client_get_database(client, db.c_str());
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
            std::cout << "errd " << json << std::endl;

            fprintf (stderr, "bson err: %s\n", error.message);
            return nullptr;
        }

        return bson;
    }

    void insert_bson_1(const std::string& script_host, bson_t* bs)
    {
        if(script_host != last_collection)
            return;

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

    std::vector<std::string> find_bson(const std::string& script_host, bson_t* bs, bson_t* ps)
    {
        std::vector<std::string> results;

        if(script_host != last_collection)
            return results;

        if(bs == nullptr)
            return results;

        const bson_t *doc;

        ///hmm. for .first() we should limit to one doc
        ///for .count we need to run a completely separate query
        ///for array, we need to do everythang
        mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, bs, ps, nullptr);

        while(mongoc_cursor_more(cursor) && mongoc_cursor_next (cursor, &doc))
        {
            char* str = bson_as_json(doc, NULL);

            results.push_back(str);

            bson_free(str);
        }

        mongoc_cursor_destroy(cursor);
    }

    std::vector<std::string> find_json(const std::string& script_host, const std::string& json, const std::string& proj)
    {
        std::vector<std::string> results;

        if(script_host != last_collection)
            return results;

        bson_t* bs = make_bson_from_json(json);
        bson_t* ps = make_bson_from_json(proj);

        results = find_bson(script_host, ps, bs);

        if(ps)
            bson_destroy(ps);

        bson_destroy(bs);

        return results;
    }

    void remove_json(const std::string& script_host, const std::string& json)
    {
        if(script_host != last_collection)
            return;

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

#include "mongo_cleanup.h"

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
