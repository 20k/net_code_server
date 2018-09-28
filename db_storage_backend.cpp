#include "db_storage_backend.hpp"
#include "mongo.hpp"
#include "user.hpp"
#include <secret/npc_manager.hpp>

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

bool is_exists(const nlohmann::json& data, const std::string& key)
{
    if(data.count(key) == 0)
        return false;

    nlohmann::json val = data.at(key);

    if(val.count("$exists") > 0)
    {
        nlohmann::json js = val.at("$exists");

        if(js.is_number())
            return js == 1;

        if(js.is_boolean())
            return js == true;

        return false;
    }

    return false;
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
    std::map<std::string, std::map<std::string, nlohmann::json>> index_map;

    std::mutex all_coll_guard;

    std::map<std::string, std::mutex> per_collection_lock;

    std::vector<nlohmann::json>& get_collection(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return all_data[coll];
    }

    std::map<std::string, nlohmann::json>& get_indexed(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return index_map[coll];
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
    std::array<std::string, (int)mongo_database_type::MONGO_COUNT> indices;

    /*std::vector<nlohmann::json>& get_collection(const std::string& db, const std::string& coll)
    {
        std::lock_guard guard(db_lock);

        return all_data[db][coll];
    }*/

    database& get_db(const database_type& db)
    {
        return all_data[db];
    }

    bool has_index(const database_type& db)
    {
        return indices[db].size() > 0;
    }

    std::string get_index(const database_type& db)
    {
        return indices[db];
    }

    void insert_one(const database_type& db, const std::string& coll, const nlohmann::json& js)
    {
        if(db_storage_backend::contains_banned_query(js))
            return;

        //std::lock_guard guard(db_lock);

        database& cdb = get_db(db);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        if(!has_index(db))
        {
            collection.push_back(js);
        }
        else
        {
            std::string index = get_index(db);

            assert(js.count(index) > 0);

            indices[js.at(index)] = js;
        }
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
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        if(!has_index(db))
        {
            for(nlohmann::json& js : collection)
            {
                if(matches(js, selector))
                {
                    updater(js, update);

                    return;
                }
            }
        }
        else
        {
            std::string index = get_index(db);

            assert(selector.count(index) > 0);

            assert(!is_exists(selector, index));

            auto found = indices.find(selector.at(index));

            if(found == indices.end())
                return;

            updater(found->second, update);
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
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        if(!has_index(db))
        {
            for(nlohmann::json& js : collection)
            {
                if(matches(js, selector))
                {
                    updater(js, update);
                }
            }
        }
        else
        {
            std::string index = get_index(db);

            assert(selector.count(index) > 0);

            assert(!is_exists(selector, index));

            auto found = indices.find(selector.at(index));

            if(found == indices.end())
                return;

            updater(found->second, update);
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
        const std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        if(!has_index(db))
        {
            for(const nlohmann::json& js : collection)
            {
                if(matches(js, selector))
                {
                    ret.push_back(js);
                }
            }
        }
        else
        {
            std::string index = get_index(db);

            assert(selector.count(index) > 0);

            if(is_exists(selector, index))
            {
                for(auto& i : indices)
                {
                    ret.push_back(i.second);
                }
            }
            else
            {
                ///throwing
                auto found = indices.find(selector.at(index));

                if(found == indices.end())
                    return {};

                ret.push_back(found->second);
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

            if(options.count("projection") > 0)
            {
                for(auto& i : ret)
                {
                    i = project(i, options.at("projection"));
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
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        if(!has_index(db))
        {
            collection.erase( std::remove_if(collection.begin(), collection.end(), [&](const nlohmann::json& js){return matches(js, selector);}), collection.end() );
        }
        else
        {
            std::string index = get_index(db);

            assert(selector.count(index) > 0);

            if(is_exists(selector, index))
            {
                indices.clear();
            }
            else
            {
                auto found = indices.find(selector.at(index));

                if(found == indices.end())
                    return;

                indices.erase(found);
            }
        }
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

    db_storage_backend::run_tests();

    for(int idx=0; idx < (int)mongo_database_type::MONGO_COUNT; idx++)
    {
        get_db_storage().all_data[(int)mongo_databases[idx]->last_db_type];
    }

    get_db_storage().indices[(int)mongo_database_type::USER_PROPERTIES] = "name";
    get_db_storage().indices[(int)mongo_database_type::USER_ITEMS] = "item_id";
    get_db_storage().indices[(int)mongo_database_type::NPC_PROPERTIES] = "name";

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

            if(!store.has_index((int)ctx->last_db_type))
            {
                store.all_data[(int)ctx->last_db_type].all_data[collection] = js;
            }
            else
            {
                std::string index = store.get_index((int)ctx->last_db_type);

                for(auto& k : js)
                {
                    assert(k.count(index) > 0);

                    std::string current_idx = k.at(index);

                    std::map<std::string, nlohmann::json>& indices = store.all_data[(int)ctx->last_db_type].index_map[collection];

                    indices[current_idx] = k;
                }
            }
        }
    }


    /*{
        for_each_user([](user& usr)
                      {

                      });

        for_each_npc([](npc_user& usr)
                     {
                        mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

                        npc_prop_list props;

                        props.load_from_db(ctx, usr.name);
                     });

        //mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

        //user usr;
        //usr.load_from_db(ctx, "i20k");
    }*/

    std::cout << "imported from mongo\n";

    //exit(0);
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

    ///compatability tests
    {
        mongo_requester req1;
        req1.set_prop_int("prop", 1);

        mongo_requester req2;
        req2.set_prop_double("prop", 1);

        //uint64_t val = ((uint64_t)1 << 63) - 1;

        //mongo_requester req3;
        //req3.set_prop_int("prop", val);

        //mongo_requester req4;
        //req4.set_prop("prop", val);

        //std::cout << req1.get_all_properties_json() << std::endl;
        //std::cout << req2.get_all_properties_json() << std::endl;

        //std::cout << req3.get_all_properties_json() << std::endl;
        //std::cout << req4.get_all_properties_json() << std::endl;

        assert(req1.get_all_properties_json() == req2.get_all_properties_json());
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
