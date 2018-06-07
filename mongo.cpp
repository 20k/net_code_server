#include "mongo.hpp"
#include <json/json.hpp>

//std::mutex mongo_context::found;

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
