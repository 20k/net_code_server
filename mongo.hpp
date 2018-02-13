#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <mongoc/mongoc.h>

struct mongo_context
{
    mongo_context(const std::string& coll)
    {
        std::string uri_str = "mongodb://user_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";

        mongoc_init();

        client = mongoc_client_new(uri_str.c_str());

        mongoc_client_set_appname(client, "crapmud");

        database = mongoc_client_get_database (client, "user_dbs");
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
            fprintf (stderr, "%s\n", error.message);
            return nullptr;
        }

        //string = bson_as_canonical_extended_json (bson, NULL);
        //printf ("%s\n", string);
        //bson_free (string);

        return bson;
    }

    void insert_json_1(const std::string& json)
    {
        bson_t* bs = make_bson_from_json(json);

        if(bs == nullptr)
            return;

        bson_error_t error;

        if (!mongoc_collection_insert_one (collection, bs, NULL, NULL, &error))
        {
            fprintf (stderr, "%s\n", error.message);
        }

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
        mongoc_collection_destroy (collection);
        mongoc_database_destroy (database);
        mongoc_client_destroy (client);

        mongoc_cleanup();
    }

    mongoc_client_t* client = nullptr;
    mongoc_database_t* database = nullptr;
    mongoc_collection_t* collection = nullptr;
};


void mongo_tests(const std::string& coll)
{
    ///mongoc_client_t *client = mongoc_client_new ("mongodb://user:password@localhost/?authSource=mydb");

    mongo_context ctx(coll);

    //ctx.insert_test_data();

    ctx.insert_json_1("{\"name\": {\"first\":\"bum\", \"last\":\"test\"}}");
}

#endif // MONGO_HPP_INCLUDED
