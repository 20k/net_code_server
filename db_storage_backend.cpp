#include "db_storage_backend.hpp"
#include "mongo.hpp"
#include "user.hpp"
#include <secret/npc_manager.hpp>

#include <tinydir/tinydir.h>
#include <direct.h>
#include <fstream>
#include <libncclient/nc_util.hpp>

#define ROOT_STORE "C:/net_code_storage"
#define ROOT_FILE "C:/net_code_storage/gid"

#include "rate_limiting.hpp"
#include "stacktrace.hpp"

template<typename T>
void for_each_dir(const std::string& directory, const T& t)
{
    tinydir_dir dir;
    tinydir_open(&dir, directory.c_str());

    while(dir.has_next)
    {
        tinydir_file file;
        tinydir_readfile(&dir, &file);

        if(file.is_dir)
        {
            std::string dir_name(file.name);

            if(dir_name != "." && dir_name != "..")
                t(dir_name);
        }

        tinydir_next(&dir);
    }

    tinydir_close(&dir);
}

struct tinydir_autoclose
{
    tinydir_dir dir;

    tinydir_autoclose(const std::string& directory)
    {
        tinydir_open(&dir, directory.c_str());
    }

    ~tinydir_autoclose()
    {
        tinydir_close(&dir);
    }
};

template<typename T>
void for_each_file(const std::string& directory, const T& t)
{
    tinydir_autoclose close(directory);

    while(close.dir.has_next)
    {
        tinydir_file file;
        tinydir_readfile(&close.dir, &file);

        if(!file.is_dir)
        {
            std::string file_name(file.name);

            if(file_name.find('.') == std::string::npos)
                t(file_name);
        }

        tinydir_next(&close.dir);
    }
}

std::string get_filename(const database_type& db, const std::string& coll, const nlohmann::json& data)
{
    std::string root = ROOT_STORE;

    std::string db_dir = root + "/" + std::to_string((int)db);

    std::string collection_dir = db_dir + "/" + coll;

    std::string final_dir = collection_dir + "/" + std::to_string((size_t)data.at(CID_STRING));

    return final_dir;
}

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

            if(val.count("$lt") > 0)
            {
                const nlohmann::json& lt_val = val.at("$lt");

                if(data.count(key) == 0)
                    return false;

                if(data.at(key) >= lt_val)
                    return false;

                continue;
            }

            if(val.count("$gt") > 0)
            {
                const nlohmann::json& gt_val = val.at("$gt");

                if(data.count(key) == 0)
                    return false;

                if(data.at(key) <= gt_val)
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

    ///this doesn't make sense, the danger is if they want to $set cid_string
    //assert(update.count(CID_STRING) == 0);

    const nlohmann::json& to_set = update.at("$set");

    if(!to_set.is_object())
    {
        printf("$set is not an object\n");

        return;
    }

    for(auto& individual_data : to_set.get<nlohmann::json::object_t>())
    {
        if(individual_data.first == CID_STRING)
            continue;

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

using mutex_t = safe_mutex;

struct database
{
    std::map<std::string, std::vector<nlohmann::json>> all_data;
    std::map<std::string, std::map<std::string, nlohmann::json>> index_map;
    std::map<std::string, bool> collection_imported;

    mutex_t all_coll_guard;

    std::map<std::string, mutex_t> per_collection_lock;

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

    std::vector<nlohmann::json>& get_collection_nolock(const std::string& coll)
    {
        return all_data[coll];
    }

    std::map<std::string, nlohmann::json>& get_indexed_nolock(const std::string& coll)
    {
        return index_map[coll];
    }

    mutex_t& get_lock(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return per_collection_lock[coll];
    }

    mutex_t& get_db_lock()
    {
        return all_coll_guard;
    }

    bool is_imported(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return collection_imported[coll];
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

    size_t global_id = 0;
    mutex_t id_guard;

    bool atomic_enabled = true;

    template<typename T>
    void atomic_write(const std::string& file, const T& data)
    {
        if(data.size() == 0)
            return;

        ///hmm
        ///<filesystem> defines some interesting semantics
        ///boo! doesn't actually work!
        if(atomic_enabled)
        {
            atomic_write_all(file, data);
        }
        else
        {
            no_atomic_write_all(file, data);
        }
    }

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

    size_t get_next_id()
    {
        std::lock_guard guard(id_guard);

        size_t val = global_id++;

        atomic_write(ROOT_FILE, std::to_string(global_id));

        return val;
    }

    ///todo: make atomic
    void flush(const database_type& db, const std::string& coll, const nlohmann::json& data)
    {
        assert(data.count(CID_STRING) == 1);

        assert(coll.find('/') == std::string::npos);

        std::string root = ROOT_STORE;

        std::string db_dir = root + "/" + std::to_string((int)db);

        mkdir(db_dir.c_str());

        std::string collection_dir = db_dir + "/" + coll;

        mkdir(collection_dir.c_str());

        std::string final_dir = collection_dir + "/" + std::to_string((size_t)data.at(CID_STRING));

        //std::string dumped = data.dump();

        std::vector<uint8_t> dumped = nlohmann::json::to_cbor(data);

        if(dumped.size() == 0)
            return;

        atomic_write(final_dir, dumped);
    }

    void disk_erase(const database_type& db, const std::string& coll, const nlohmann::json& data)
    {
        assert(data.count(CID_STRING) > 0);

        remove(get_filename(db, coll, data).c_str());

        ///don't care about notifs and they're taking up waaaay too much space
        if((mongo_database_type)db == mongo_database_type::PENDING_NOTIFS)
            remove((get_filename(db, coll, data) + ".back").c_str());
    }

    ///must have collection locked to work
    ///internally locks whole db
    void import_collection_nolock(const database_type& db_idx, const std::string& coll)
    {
        //std::lock_guard guard(cdb.get_lock(coll));

        database& cdb = get_db(db_idx);

        {
            std::lock_guard guard(cdb.get_db_lock());

            if(cdb.collection_imported[coll])
                return;
        }

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        collection.clear();
        indices.clear();

        std::string coll_path = std::string(ROOT_STORE) + "/" + std::to_string((int)db_idx) + "/" + coll;

        tinydir_dir dir;

        if(tinydir_open(&dir, coll_path.c_str()) == -1)
        {
            {
                std::lock_guard guard(cdb.get_db_lock());

                cdb.collection_imported[coll] = true;
            }

            return;
        }

        tinydir_close(&dir);

        for_each_file(coll_path, [&](const std::string& file_name)
        {
            COOPERATE_KILL_THREAD_LOCAL();

            std::string path = coll_path + "/" + file_name;

            std::string data = read_file_bin(path);

            nlohmann::json fdata;

            try
            {
                fdata = nlohmann::json::from_cbor(data);
            }
            catch(...)
            {
                std::cout << "bad coll " << coll_path << " file name " << file_name << std::endl;
                std::cout << "raw dlen " << data.size() << std::endl;

                /*if(file_exists(path + ".back"))
                {
                    try
                    {
                        fdata = nlohmann::json::from_cbor(read_file_bin(path + ".back"));

                        std::cout << "successfully recovered data from backup" << std::endl;
                    }
                    catch(...)
                    {
                        throw std::runtime_error("Bad collection and no backup");
                    }
                }*/

                throw std::runtime_error("Db corruption oops!");
            }

            if(!has_index(db_idx))
            {
                collection.push_back(fdata);
            }
            else
            {
                std::string index = get_index(db_idx);

                assert(fdata.count(index) > 0);

                std::string current_idx = fdata.at(index);

                //std::map<std::string, nlohmann::json>& indices = all_data[db_idx].index_map[coll];

                indices[current_idx] = fdata;
            }
        });

        {
            std::lock_guard guard(cdb.get_db_lock());

            cdb.collection_imported[coll] = true;
        }
    }

    void insert_one(const database_type& db, const std::string& coll, const nlohmann::json& js)
    {
        if(db_storage_backend::contains_banned_query(js))
            return;

        database& cdb = get_db(db);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        std::lock_guard guard(cdb.get_lock(coll));

        ///nothing can touch the collection while we do this so its fine
        bool is_imported = cdb.is_imported(coll);

        auto fdata = js;
        fdata[CID_STRING] = get_next_id();

        if(!has_index(db))
        {
            if(is_imported)
                collection.push_back(fdata);

            flush(db, coll, fdata);
        }
        else
        {
            std::string index = get_index(db);

            assert(fdata.count(index) > 0);

            if(is_imported)
                indices[fdata.at(index)] = fdata;

            flush(db, coll, fdata);
        }
    }

    template<typename T>
    void for_each_match_nolock(const database_type& db, const std::string& coll, const nlohmann::json& selector, const T& t)
    {
        database& cdb = get_db(db);

        //std::lock_guard guard(cdb.get_lock(coll));

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        if(!has_index(db))
        {
            for(nlohmann::json& js : collection)
            {
                if(matches(js, selector))
                {
                    if(t(js))
                        return;
                }
            }
        }
        else
        {
            std::string index = get_index(db);

            bool has_index_key = selector.count(index) > 0;

            if(has_index_key)
            {
                if(is_exists(selector, index))
                {
                    for(auto& i : indices)
                    {
                        if(matches(i.second, selector))
                        {
                            if(t(i.second))
                                return;
                        }
                    }
                }
                else
                {
                    ///throwing
                    auto found = indices.find(selector.at(index));

                    if(found == indices.end())
                        return;

                    if(matches(found->second, selector))
                    {
                        if(t(found->second))
                            return;
                    }
                }
            }
            else
            {
                for(auto& i : indices)
                {
                    if(matches(i.second, selector))
                    {
                        if(t(i.second))
                            return;
                    }
                }
            }
        }
    }

    ///ensure that update can never contain CID_STRING
    void update_one(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& update)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return;

        if(db_storage_backend::contains_banned_query(update))
            return;

        /*if(update.count("parsed_source") > 0)
        {
            std::cout << "update " << update.at("parsed_source") << std::endl;
        }*/

        /*bool checky = false;

        if(update.count("$set") > 0)
        {
            if(update.at("$set").count("parsed_source") > 0)
            {
                std::cout << update.at("$set").at("parsed_source") << std::endl;

                checky = true;
            }
        }*/

        database& cdb = get_db(db);

        std::lock_guard guard(cdb.get_lock(coll));

        import_collection_nolock(db, coll);

        for_each_match_nolock(db, coll, selector, [&](nlohmann::json& js)
        {
            /*if(checky)
            {
                std::cout << "Checkers!\n";

                std::cout << "old parsed" << js.at("parsed_source") << std::endl;
            }*/

            updater(js, update);

            /*if(checky)
            {
                std::cout << "new parsed " << js << std::endl;
            }*/

            flush(db, coll, js);
            return true;
        });
    }

    void update_many(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& update)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return;

        if(db_storage_backend::contains_banned_query(update))
            return;

        database& cdb = get_db(db);

        std::lock_guard guard(cdb.get_lock(coll));

        import_collection_nolock(db, coll);

        for_each_match_nolock(db, coll, selector, [&](nlohmann::json& js)
        {
            updater(js, update);

            flush(db, coll, js);

            return false;
        });
    }

    std::vector<nlohmann::json> find_many(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& options)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return std::vector<nlohmann::json>();

        if(db_storage_backend::contains_banned_query(options))
            return std::vector<nlohmann::json>();

        std::vector<nlohmann::json> ret;

        {
            database& cdb = get_db(db);

            std::lock_guard guard(cdb.get_lock(coll));

            import_collection_nolock(db, coll);

            for_each_match_nolock(db, coll, selector, [&](nlohmann::json& js)
            {
                ///abort if runtime runs significantly over
                COOPERATE_KILL_THREAD_LOCAL_URGENT();

                ret.push_back(js);

                return false;
            });
        }

        if(options.is_object())
        {
            if(options.count("sort") > 0)
            {
                for(auto& pairs : options.at("sort").get<nlohmann::json::object_t>())
                {
                    std::string sort_on = pairs.first;

                    int direction = pairs.second;

                    std::sort(ret.begin(), ret.end(), [&](const nlohmann::json& n_1, const nlohmann::json& n_2)
                              {
                                  if(direction == 1)
                                    return n_1.at(sort_on) < n_2.at(sort_on);
                                  else
                                    return n_1.at(sort_on) > n_2.at(sort_on);
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

            if(options.count("limit") > 0)
            {
                nlohmann::json lim = options.at("limit");

                if(lim.is_number())
                {
                    int val = (int)lim;

                    if(val >= 0 && (int)ret.size() > val)
                    {
                        ret.resize(val);
                    }
                }
            }
        }

        return ret;
    }

    void remove_many(const database_type& db, const std::string& coll, const nlohmann::json& selector)
    {
        if(db_storage_backend::contains_banned_query(selector))
            return;

        database& cdb = get_db(db);

        std::lock_guard guard(cdb.get_lock(coll));

        import_collection_nolock(db, coll);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);
        std::map<std::string, nlohmann::json>& indices = cdb.get_indexed(coll);

        if(!has_index(db))
        {
            for(auto& js : collection)
            {
                if(matches(js, selector))
                {
                    disk_erase(db, coll, js);
                }
            }

            collection.erase( std::remove_if(collection.begin(), collection.end(), [&](const nlohmann::json& js){return matches(js, selector);}), collection.end() );
        }
        else
        {
            std::string index = get_index(db);

            if(is_exists(selector, index) || selector.count(index) == 0)
            {
                for(auto it = indices.begin(); it != indices.end(); )
                {
                    if(matches(it->second, selector))
                    {
                        disk_erase(db, coll, it->second);

                        it = indices.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
            }
            else
            {
                auto found = indices.find(selector.at(index));

                if(found == indices.end())
                    return;

                disk_erase(db, coll, found->second);

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

void import_from_mongo()
{
    #ifdef USE_MONGO
    db_storage& store = get_db_storage();

    store.global_id = 0;

    store.atomic_enabled = false;

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

                //if(found.count(CID_STRING) == 0)
                {
                    found[CID_STRING] = store.get_next_id();
                }

                js.push_back(found);
            }

            if(!store.has_index((int)ctx->last_db_type))
            {
                store.all_data[(int)ctx->last_db_type].all_data[collection] = js;

                for(auto& k : js)
                {
                    store.flush((int)ctx->last_db_type, collection, k);
                }
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

                    store.flush((int)ctx->last_db_type, collection, k);
                }
            }

            store.all_data[(int)ctx->last_db_type].collection_imported[collection] = true;
        }
    }

    store.atomic_enabled = true;

    std::cout << "imported from mongo\n";
    #else
    throw std::runtime_error("Should not be possible (use mongo import from mongo)");
    #endif

    //exit(0);
}

void import_from_disk()
{
    db_storage& store = get_db_storage();

    std::string root = ROOT_STORE;

    for(int db_idx=0; db_idx < (int)mongo_database_type::MONGO_COUNT; db_idx++)
    {
        std::string db_dir = root + "/" + std::to_string((int)db_idx);

        /*for_each_dir(db_dir, [&](const std::string& coll)
        {
            for_each_file(db_dir + "/" + coll, [&](const std::string& file_name)
            {
                std::string path = db_dir + "/" + coll + "/" + file_name;

                std::string data = read_file_bin(path);

                nlohmann::json fdata = nlohmann::json::from_cbor(data);

                if(!store.has_index(db_idx))
                {
                    store.all_data[db_idx].all_data[coll].push_back(fdata);
                }
                else
                {
                    std::string index = store.get_index(db_idx);

                    assert(fdata.count(index) > 0);

                    std::string current_idx = fdata.at(index);

                    std::map<std::string, nlohmann::json>& indices = store.all_data[db_idx].index_map[coll];

                    indices[current_idx] = fdata;
                }

                store.all_data[db_idx].collection_imported[coll] = true;
            });
        });*/
    }

    std::cout << "imported from disk" << std::endl;
}

void init_db_storage_backend()
{
    ///importa data from mongo

    db_storage_backend::run_tests();

    db_storage& store = get_db_storage();

    mkdir(ROOT_STORE);

    std::string root_file = ROOT_FILE;

    std::string resulting_data = read_file(root_file);

    std::cout << "RDATA " << resulting_data << std::endl;

    bool new_data = false;

    if(resulting_data.size() == 0)
    {
        throw std::runtime_error("Yeah we're past this point! file explosion");

        store.atomic_write(root_file, std::to_string(0));
        //write_all(root_file, std::to_string(0));
    }
    else
    {
        store.global_id = atoll(resulting_data.c_str());

        new_data = true;
    }

    ///READ ID STORAGE FROM DISK
    ///obviously unimplemented now

    for(int idx=0; idx < (int)mongo_database_type::MONGO_COUNT; idx++)
    {
        store.all_data[(int)mongo_databases[idx]->last_db_type];
    }

    store.indices[(int)mongo_database_type::USER_PROPERTIES] = "name";
    store.indices[(int)mongo_database_type::USER_ITEMS] = "item_id";
    store.indices[(int)mongo_database_type::NPC_PROPERTIES] = "name";
    store.indices[(int)mongo_database_type::NETWORK_PROPERTIES] = "name";

    if(!new_data)
    {
        import_from_mongo();
    }
    else
    {
        import_from_disk();
    }

    ///error with "unique_id" : "fragme_vgdajw_6153"

    /*{
        for_each_user([](user& usr)
                      {

                      });

        for_each_npc([](npc_user& usr)
                     {
                         {
                            mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

                            npc_prop_list props;

                            props.load_from_db(ctx, usr.name);
                         }

                         {
                            get_nodes(usr.name, -2);
                         }
                     });

        //mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

        //user usr;
        //usr.load_from_db(ctx, "i20k");
    }*/


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

std::mutex& db_storage_backend::get_lock_for()
{
    return get_db_storage().get_db(database).get_lock(collection).mutex;
}

std::vector<nlohmann::json>& db_storage_backend::get_db_data_nolock_import()
{
    get_db_storage().import_collection_nolock(database, collection);

    return get_db_storage().get_db(database).get_collection_nolock(collection);
}

void db_storage_backend::flush(const nlohmann::json& data)
{
    return get_db_storage().flush(database, collection, data);
}

void db_storage_backend::disk_erase(const nlohmann::json& data)
{
    return get_db_storage().disk_erase(database, collection, data);
}

size_t db_storage_backend::get_unique_id()
{
    return get_db_storage().get_next_id();
}

void remove_mongo_id(nlohmann::json& in)
{
    if(in.count("_id") > 0)
    {
        in.erase(in.find("_id"));
    }

    if(in.count(CID_STRING) > 0)
    {
        in.erase(in.find(CID_STRING));
    }
}
