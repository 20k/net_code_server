#include "db_storage_backend.hpp"
#include "mongo.hpp"

bool matches(const nlohmann::json& data, const nlohmann::json& match)
{
    if(!match.is_object())
        return data == match;

    for(auto& match : match.get<nlohmann::json::object_t>())
    {
        const std::string& key = match.first;
        const nlohmann::json& val = match.second;

        if(val.is_object())
        {
            if(val.count("$exists") > 0)
            {
                const nlohmann::json& exist_val = val.at("$exists");

                bool truthy = false;

                if(exist_val.is_boolean())
                {
                    truthy = (bool)exist_val;
                }

                if(exist_val.is_number())
                {
                    truthy = (int)exist_val;
                }

                if(truthy && data.count(key) == 0)
                    return false;

                if(!truthy && data.count(key) > 0)
                    return false;

                continue;
            }
        }

        if(data.count(key) == 0)
            return false;

        if(data.at(key) != val)
            return false;
    }

    return true;
}

struct db_storage
{
    std::map<std::string, std::map<std::string, std::vector<nlohmann::json>>> all_data;

    std::mutex db_lock;

    std::vector<nlohmann::json>& get_collection(const std::string& db, const std::string& coll)
    {
        std::lock_guard guard(db_lock);

        return all_data[db][coll];
    }

    void insert_1(const std::string& db, const std::string& coll, const nlohmann::json& js)
    {
        std::lock_guard guard(db_lock);

        all_data[db][coll].push_back(js);
    }

    void update_one(const std::string& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& update)
    {
        std::lock_guard guard(db_lock);

        std::vector<nlohmann::json>& collection = all_data[db][coll];


    }
};

db_storage& get_db_storage()
{
    static db_storage store;

    return store;
}

void init_db_storage_backend()
{

}

void db_storage_backend::run_tests()
{
    {
        nlohmann::json js;
        js["$where"] = "something";

        assert(db_storage_backend::contains_banned_query(js) == true);
    }

    {
        nlohmann::json js;
        js["poop"] = "something";

        assert(db_storage_backend::contains_banned_query(js) == false);
    }

    {
        nlohmann::json js = 1;

        assert(db_storage_backend::contains_banned_query(js) == false);
    }

    {
        nlohmann::json js{1, 2, 3};

        assert(db_storage_backend::contains_banned_query(js) == false);
    }


    {
        db_storage_backend backend(mongo_databases[(int)mongo_database_type::USER_PROPERTIES]);

        backend.change_collection_unsafe("i20k");

        assert(backend.collection == "i20k");
        assert(backend.database == "user_properties");
    }
}

/*void mongo_interface::change_collection_unsafe(const std::string& coll, bool force_change)
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
}*/

bool db_storage_backend::contains_banned_query(nlohmann::json& js)
{
    std::vector<std::string> banned
    {
        "$where",
        "$expr",
        "$maxTimeMS",
        "$query",
        "$showDiskLoc"
    };

    if(!js.is_object())
        return false;

    try
    {
        for(auto& i : js.get<nlohmann::json::object_t>())
        {
            for(auto& k : banned)
            {
                if(i.first == k)
                    return true;
            }
        }
    }
    catch(...)
    {
        printf("Banned query exception\n");
        return true;
    }

    return false;
}

void db_storage_backend::change_collection_unsafe(const std::string& coll, bool force_change)
{
    if(ctx->is_fixed && !force_change)
    {
        std::cout << "warning, collection should not be changed" << std::endl;
        return;
    }

    if(coll == collection && !force_change)
        return;

    collection = coll;
}

db_storage_backend::db_storage_backend(mongo_context* fctx)
{
    ctx = fctx;
    database = fctx->last_db;
}
