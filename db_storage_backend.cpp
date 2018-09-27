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

void updater(nlohmann::json& data, const nlohmann::json& update)
{
    if(update.count("$set") == 0)
    {
        printf("no $set in update\n");

        return;

        //throw std::runtime_error("You probably want a $set in your update query");
    }

    const nlohmann::json& to_set = update.at("$set");

    if(!to_set.is_object())
    {
        printf("$set is not an object\n");

        return;
    }

    for(auto& individual_data : to_set.get<nlohmann::json::object_t>())
    {
        data[individual_data.first] = individual_data.second;
    }
}

nlohmann::json project(const nlohmann::json& data, const nlohmann::json& proj)
{
    if(!proj.is_object())
        return data;

    int num = 0;

    for(auto& i __attribute__((unused)) : proj.get<nlohmann::json::object_t>())
    {
        num++;
    }

    if(num == 0)
        return data;

    nlohmann::json ret;

    for(auto& individual_data : proj.get<nlohmann::json::object_t>())
    {
        bool truthy = false;

        if(individual_data.second.is_boolean())
        {
            truthy = (bool)individual_data.second;
        }

        if(individual_data.second.is_number())
        {
            truthy = (int)individual_data.second;
        }

        if(data.count(individual_data.first) == 0)
            continue;

        if(truthy)
        {
            ret[individual_data.first] = data.at(individual_data.first);
        }
    }

    return ret;
}

struct database
{
    std::map<std::string, std::vector<nlohmann::json>> all_data;

    std::mutex all_coll_guard;

    std::map<std::string, std::mutex> per_collection_lock;

    std::vector<nlohmann::json>& get_collection(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return all_data[coll];
    }

    std::mutex& get_lock(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return per_collection_lock[coll];
    }
};

bool json_prop_true(const nlohmann::json& js, const std::string& key)
{
    if(js.count(key) == 0)
        return false;

    if(js.at(key).is_boolean())
    {
        return js.at(key) == true;
    }

    if(js.at(key).is_number())
    {
        return js.at(key) == 1;
    }

    return false;
}

struct db_storage
{
    //std::map<std::string, std::map<std::string, std::vector<nlohmann::json>>> all_data;

    //std::mutex db_lock;

    //std::map<database_type, database> all_data;

    std::array<database, (int)mongo_database_type::MONGO_COUNT> all_data;

    /*std::vector<nlohmann::json>& get_collection(const std::string& db, const std::string& coll)
    {
        std::lock_guard guard(db_lock);

        return all_data[db][coll];
    }*/

    database& get_db(const database_type& db)
    {
        return all_data[db];
    }

    void insert_one(const database_type& db, const std::string& coll, const nlohmann::json& js)
    {
        if(db_storage_backend::contains_banned_query(js))
            return;

        //std::lock_guard guard(db_lock);

        database& cdb = get_db(db);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        collection.push_back(js);

        //all_data[db][coll].push_back(js);
    }

    void update_one(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& update)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return;

        if(db_storage_backend::contains_banned_query(update))
            return;

        //std::lock_guard guard(db_lock);

        //std::vector<nlohmann::json>& collection = all_data[db][coll];

        database& cdb = get_db(db);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        for(nlohmann::json& js : collection)
        {
            if(matches(js, selector))
            {
                updater(js, update);

                return;
            }
        }
    }

    void update_many(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& update)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return;

        if(db_storage_backend::contains_banned_query(update))
            return;

        //std::lock_guard guard(db_lock);

        //std::vector<nlohmann::json>& collection = all_data[db][coll];

        database& cdb = get_db(db);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        for(nlohmann::json& js : collection)
        {
            if(matches(js, selector))
            {
                updater(js, update);
            }
        }
    }

    std::vector<nlohmann::json> find_many(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& options)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return std::vector<nlohmann::json>();

        if(db_storage_backend::contains_banned_query(options))
            return std::vector<nlohmann::json>();

        //std::lock_guard guard(db_lock);

        std::vector<nlohmann::json> ret;

        //const std::vector<nlohmann::json>& collection = all_data[db][coll];

        database& cdb = get_db(db);

        const std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        for(const nlohmann::json& js : collection)
        {
            if(matches(js, selector))
            {
                //auto res = project(js, projector);

                ///ok this is incorrect
                ///the way mongoc does it is that this is an options structure

                ret.push_back(js);
            }
        }

        if(options.is_object())
        {
            if(options.count("sort") > 0)
            {
                for(auto& pairs : options.at("sort").get<nlohmann::json::object_t>())
                {
                    std::string sort_on = pairs.first;

                    std::sort(ret.begin(), ret.end(), [&](const nlohmann::json& n_1, nlohmann::json& n_2)
                              {
                                return n_1.at(sort_on) < n_2.at(sort_on);
                              });

                    break;
                }
            }
        }

        return ret;
    }

    void remove_many(const database_type& db, const std::string& coll, const nlohmann::json& selector)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return;

        //std::lock_guard guard(db_lock);

        //std::vector<nlohmann::json>& collection = all_data[db][coll];

        database& cdb = get_db(db);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        collection.erase( std::remove_if(collection.begin(), collection.end(), [&](const nlohmann::json& js){return matches(js, selector);}), collection.end() );
    }
};

db_storage& get_db_storage()
{
    static db_storage store;

    return store;
}

void init_db_storage_backend()
{
    ///importa data from mongo

    for(int idx=0; idx < (int)mongo_database_type::MONGO_COUNT; idx++)
    {
        get_db_storage().all_data[(int)mongo_databases[idx]->last_db_type];
    }

    for(int idx=0; idx < (int)mongo_database_type::MONGO_COUNT; idx++)
    {
        mongo_context* ctx = mongo_databases[idx];

        mongo_nolock_proxy mongo_ctx = get_global_mongo_context((mongo_database_type)idx, -2);
        mongo_ctx.ctx.enable_testing_backend = false;

        for(const std::string& collection : ctx->all_collections)
        {
            mongo_ctx.change_collection(collection, true);

            std::vector<std::string> all = mongo_ctx->find_json(mongo_ctx->last_collection, "{}", "{}");

            std::vector<nlohmann::json> js;

            for(auto& i : all)
            {
                nlohmann::json found = nlohmann::json::parse(i);

                js.push_back(found);

                /*if(idx == (int)mongo_database_type::GLOBAL_PROPERTIES)
                {
                    std::cout << "fi " << i << std::endl;
                }*/
            }

            db_storage& store = get_db_storage();

            //std::lock_guard guard(store.db_lock);

            store.all_data[(int)ctx->last_db_type].all_data[collection] = js;
        }
    }

    std::cout << "imported from mongo\n";

    db_storage_backend::run_tests();
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
        assert(backend.database == (int)mongo_database_type::USER_PROPERTIES);
        //assert(backend.database == "user_properties");
    }

    nlohmann::json data;
    data["cat"] = "dog";
    data["potato"] = "ketchup";

    ///MATCHES TESTS
    {
        {
            nlohmann::json select;
            select["cat"] = "dog";

            assert(matches(data, select));
        }

        {
            nlohmann::json select;
            select["potato"] = "ketchup1";

            assert(!matches(data, select));
        }

        {
            nlohmann::json select;
            select["potato"] = "ketchu";

            assert(!matches(data, select));
        }

        {
            nlohmann::json select;
            select["potato"] = "ketchup";

            assert(matches(data, select));
        }

        {
            nlohmann::json exist;
            exist["$exists"] = true;

            nlohmann::json select;
            select["cat"] = exist;

            assert(matches(data, select));
        }

        {
            nlohmann::json exist;
            exist["$exists"] = false;

            nlohmann::json select;
            select["cat"] = exist;

            assert(!matches(data, select));
        }

        {
            nlohmann::json exist;
            exist["$exists"] = true;

            nlohmann::json select;
            select["random_key"] = exist;

            assert(!matches(data, select));
        }

        {
            nlohmann::json exist;
            exist["$exists"] = 1;

            nlohmann::json select;
            select["cat"] = exist;

            assert(matches(data, select));
        }

        {
            nlohmann::json exist;
            exist["$exists"] = 0;

            nlohmann::json select;
            select["random_key"] = exist;

            assert(matches(data, select));
        }

        {
            nlohmann::json exist;
            exist["$exists"] = "ruh roh";

            nlohmann::json select;
            select["cat"] = exist;

            assert(!matches(data, select));
        }
    }

    ///UPDATER TESTS
    {
        {
            nlohmann::json data;
            data["cat"] = "spooky";
            data["poop"] = 12;

            nlohmann::json to_update;
            to_update["weee"] = 54;
            to_update["cat"] = "boop";

            nlohmann::json setter;
            setter["$set"] = to_update;

            updater(data, setter);

            assert(data["cat"] == "boop");
            assert(data["weee"] == 54);
            assert(data["poop"] == 12);
        }
    }

    ///PROJECTION TESTS
    {
        {
            nlohmann::json data;
            data["arg_1"] = 12;
            data["arg_2"] = "hello there";
            data["arg_3"] = "asdfsdfxvbxcvlbkj";

            nlohmann::json proj;
            proj["arg_1"] = 1;
            proj["arg_2"] = 0;

            auto found = project(data, proj);

            assert(found.count("arg_1") == 1);
            assert(found["arg_1"] == 12);
            assert(found.count("arg_2") == 0);
            assert(found.count("arg_3") == 0);
        }

        {
            nlohmann::json data;
            data["arg_1"] = 12;
            data["arg_2"] = "hello there";
            data["arg_3"] = "asdfsdfxvbxcvlbkj";

            nlohmann::json proj = "{}";

            auto post_proj = project(data, proj);

            assert(post_proj == data);
        }
    }
}

bool db_storage_backend::contains_banned_query(const nlohmann::json& js)
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
    database = (int)fctx->last_db_type;
}

void db_storage_backend::insert_one(const nlohmann::json& json)
{
    get_db_storage().insert_one(database, collection, json);
}

void db_storage_backend::update_one(const nlohmann::json& selector, const nlohmann::json& update)
{
    get_db_storage().update_one(database, collection, selector, update);
}

void db_storage_backend::update_many(const nlohmann::json& selector, const nlohmann::json& update)
{
    get_db_storage().update_many(database, collection, selector, update);
}

std::vector<nlohmann::json> db_storage_backend::find_many(const nlohmann::json& selector, const nlohmann::json& options)
{
    return get_db_storage().find_many(database, collection, selector, options);
}

void db_storage_backend::remove_many(const nlohmann::json& selector)
{
    get_db_storage().remove_many(database, collection, selector);
}

void remove_mongo_id(nlohmann::json& in)
{
    if(in.count("_id") > 0)
    {
        in.erase(in.find("_id"));
    }
}
