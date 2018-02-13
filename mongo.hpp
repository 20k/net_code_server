#ifndef MONGO_HPP_INCLUDED
#define MONGO_HPP_INCLUDED

#include <mongoc/mongoc.h>

void mongo_tests(const std::string& coll)
{
    ///mongoc_client_t *client = mongoc_client_new ("mongodb://user:password@localhost/?authSource=mydb");


    std::string uri_str = "mongodb://user_database:james20kuserhandlermongofun@localhost:27017/?authSource=users";


    mongoc_database_t *database;
    mongoc_collection_t *collection;
    bson_error_t error;
    bool retval;

    /*
     * Required to initialize libmongoc's internals
     */
    mongoc_init ();

    mongoc_client_t* client = mongoc_client_new (uri_str.c_str());

    /*
     * Register the application name so we can track it in the profile logs
     * on the server. This can also be done from the URI (see other examples).
     */
    mongoc_client_set_appname (client, "crapmud");

    /*
     * Get a handle on the database "db_name" and collection "coll_name"
     */
    database = mongoc_client_get_database (client, "user_dbs");
    collection = mongoc_client_get_collection (client, "user_dbs", coll.c_str());

    /*
     * Do work. This example pings the database, prints the result as JSON and
     * performs an insert
     */
    /*bson_t* command = BCON_NEW ("ping", BCON_INT32 (1));

    bson_t reply;

    retval = mongoc_client_command_simple (
                 client, "admin", command, NULL, &reply, &error);

    if (!retval)
    {
        fprintf (stderr, "%s\n", error.message);
        //return EXIT_FAILURE;

        return;
    }*/

    //char* str = bson_as_json (&reply, NULL);
    //printf ("%s\n", str);

    bson_t* insert = BCON_NEW ("hello", BCON_UTF8 ("world"));

    if (!mongoc_collection_insert_one (collection, insert, NULL, NULL, &error))
    {
        fprintf (stderr, "%s\n", error.message);
    }

    bson_destroy (insert);
    //bson_destroy (&reply);
    //bson_destroy (command);
    //bson_free (str);

    /*
     * Release our handles and clean up libmongoc
     */
    mongoc_collection_destroy (collection);
    mongoc_database_destroy (database);
    mongoc_client_destroy (client);
    mongoc_cleanup ();
}

#endif // MONGO_HPP_INCLUDED
