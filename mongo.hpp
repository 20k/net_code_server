#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <mongoc/mongoc.h>

struct mongo_context
{
    mongoc_client_t* client = nullptr;
    mongoc_database_t* database = nullptr;
    mongoc_collection_t* collection = nullptr;

    std::string last_collection = "";

    mongo_context()
    {
        std::string uri_str = "mongodb://user_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";

        mongoc_init();

        client = mongoc_client_new(uri_str.c_str());

        mongoc_client_set_appname(client, "crapmud");

        database = mongoc_client_get_database (client, "user_dbs");
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

        collection = mongoc_client_get_collection (client, "user_dbs", coll.c_str());
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

    void insert_json_1(const std::string& script_host, const std::string& json)
    {
        if(script_host != last_collection)
            return;

        bson_t* bs = make_bson_from_json(json);

        if(bs == nullptr)
            return;

        bson_error_t error;

        if(!mongoc_collection_insert_one(collection, bs, NULL, NULL, &error))
        {
            fprintf (stderr, "err: %s\n", error.message);
        }

        bson_destroy(bs);
    }

    std::vector<std::string> find_json(const std::string& script_host, const std::string& json, const std::string& proj)
    {
        std::vector<std::string> results;

        if(script_host != last_collection)
            return results;

        bson_t* bs = make_bson_from_json(json);
        bson_t* ps = make_bson_from_json(proj);

        /*ps =  BCON_NEW ("projection", "{",
                    "doot", BCON_BOOL (false),
                 "}");*/

        //std::cout << "hi " << bson_as_json(bs, nullptr);

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
            //printf ("found %s\n", str);

            results.push_back(str);

            bson_free(str);
        }

        mongoc_cursor_destroy(cursor);

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

        mongoc_cleanup();
    }
};

static
void cleanup_mongo();

static mongo_context* get_global_mongo_context(bool destroy = false)
{
    static mongo_context* ctx = nullptr;

    if(ctx == nullptr)
    {
        ctx = new mongo_context();

        atexit(cleanup_mongo);
    }

    if(destroy)
    {
        if(ctx)
            delete ctx;

        ctx = nullptr;
    }

    return ctx;
}

static
void cleanup_mongo()
{
    get_global_mongo_context(true);
}

void mongo_tests(const std::string& coll)
{
    ///mongoc_client_t *client = mongoc_client_new ("mongodb://user:password@localhost/?authSource=mydb");

    mongo_context ctx;
    ctx.change_collection(coll);

    //ctx.insert_test_data();

    ctx.insert_json_1(coll, "{\"name\": {\"first\":\"bum\", \"last\":\"test\"}}");
}

#endif // MONGO_HPP_INCLUDED
