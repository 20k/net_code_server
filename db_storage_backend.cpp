#include "db_storage_backend.hpp"
#include "user.hpp"
#include <secret/npc_manager.hpp>

#include <tinydir/tinydir.h>
#ifdef __WIN32__
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include <fstream>
#include <libncclient/nc_util.hpp>

#ifdef __WIN32__
#define ROOT_STORE "C:/net_code_storage"
#define ROOT_FILE "C:/net_code_storage/gid"
#else
#define ROOT_STORE "/net_code_storage"
#define ROOT_FILE "/net_code_storage/gid"
#endif

#include "rate_limiting.hpp"
#include "stacktrace.hpp"
#include "directory_helpers.hpp"

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

            if(val.count("$eq") > 0)
            {
                const nlohmann::json& eq_val = val.at("$eq");

                if(data.count(key) == 0)
                    return false;

                if(data.at(key) != eq_val)
                    return false;

                continue;
            }

            if(val.count("$ne") > 0)
            {
                const nlohmann::json& eq_val = val.at("$ne");

                if(data.count(key) == 0)
                    return false;

                if(data.at(key) == eq_val)
                    return false;

                continue;
            }

            if(val.count("$lte") > 0)
            {
                const nlohmann::json& eq_val = val.at("$lte");

                if(data.count(key) == 0)
                    return false;

                if(data.at(key) > eq_val)
                    return false;

                continue;
            }

            if(val.count("$gte") > 0)
            {
                const nlohmann::json& eq_val = val.at("$gte");

                if(data.count(key) == 0)
                    return false;

                if(data.at(key) < eq_val)
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

using mutex_t = lock_type_t;

struct database
{
    std::map<std::string, std::vector<nlohmann::json>> all_data;
    std::map<std::string, std::map<size_t, size_t>> data_size;
    std::map<std::string, bool> collection_imported;

    mutex_t all_coll_guard;

    std::map<std::string, mutex_t> per_collection_lock;

    std::vector<nlohmann::json>& get_collection(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return all_data[coll];
    }


    std::vector<nlohmann::json>& get_collection_nolock(const std::string& coll)
    {
        return all_data[coll];
    }

    std::map<size_t, size_t>& get_collection_size(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        return data_size[coll];
    }

    size_t get_total_size(const std::string& coll)
    {
        std::lock_guard guard(all_coll_guard);

        size_t val = 0;

        for(auto& i : data_size[coll])
        {
            val += i.second;
        }

        return val;
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
    std::array<database, (int)mongo_database_type::MONGO_COUNT> all_data;

    int max_db_size = 16 * 1024 * 1024;

    size_t global_id = 0;
    size_t reserved_global_id = 0;
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

    size_t get_next_id()
    {
        std::lock_guard guard(id_guard);

        if(global_id == reserved_global_id)
        {
            reserved_global_id += 1024;

            atomic_write(ROOT_FILE, std::to_string(reserved_global_id));
        }

        return global_id++;
    }

    ///todo: make atomic
    void flush(const database_type& db, const std::string& coll, const nlohmann::json& data)
    {
        assert(data.count(CID_STRING) == 1);
        flush_to(ROOT_STORE, db, coll, nlohmann::json::to_cbor(data), data.at(CID_STRING));
    }

    void flush_to(const std::string& root, const database_type& db, const std::string& coll, const std::vector<uint8_t>& data, size_t cid)
    {
        assert(coll.find('/') == std::string::npos);

        {
            database& cdb = get_db(db);

            cdb.get_collection_size(coll)[cid] = data.size();

            int total_size = cdb.get_total_size(coll);

            if(total_size > max_db_size)
            {
                {
                    std::lock_guard guard(cdb.get_db_lock());

                    cdb.collection_imported[coll] = false;
                }

                throw std::runtime_error("Exceeded db storage size");
            }
        }

        std::string db_dir = root + "/" + std::to_string((int)db);

        #ifdef __WIN32__
        mkdir(db_dir.c_str());
        #else
        mkdir(db_dir.c_str(), 0777);
        #endif // __WIN32__

        std::string collection_dir = db_dir + "/" + coll;

        #ifdef __WIN32__
        mkdir(collection_dir.c_str());
        #else
        mkdir(collection_dir.c_str(), 0777);
        #endif // __WIN32__

        std::string final_dir = collection_dir + "/" + std::to_string(cid);

        if(data.size() == 0)
            return;

        atomic_write(final_dir, data);
    }

    void disk_erase(const database_type& db, const std::string& coll, const nlohmann::json& data)
    {
        assert(data.count(CID_STRING) > 0);

        {
            database& cdb = get_db(db);
            std::map<size_t, size_t>& sizes = cdb.get_collection_size(coll);
            sizes.erase(data[CID_STRING]);
        }

        remove(get_filename(db, coll, data).c_str());
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
        std::map<size_t, size_t>& collection_size = cdb.get_collection_size(coll);

        collection.clear();
        collection_size.clear();

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

        std::vector<std::string> file_names;

        for_each_file(coll_path, [&](const std::string& file_name)
        {
            file_names.push_back(file_name);
        });

        std::sort(file_names.begin(), file_names.end());

        for(const std::string& file_name : file_names)
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

                if(file_exists(path + ".back"))
                {
                    try
                    {
                        std::string backup_data = read_file_bin(path + ".back");

                        fdata = nlohmann::json::from_cbor(backup_data);

                        atomic_write(path, backup_data);

                        std::cout << "successfully recovered data from backup" << std::endl;
                    }
                    catch(...)
                    {
                        throw std::runtime_error("Bad collection and no backup");
                    }
                }
                else
                {
                    throw std::runtime_error("Db corruption oops!");
                }
            }

            collection.push_back(fdata);
            collection_size[(size_t)fdata[CID_STRING]] = data.size();
        }

        {
            std::lock_guard guard(cdb.get_db_lock());

            cdb.collection_imported[coll] = true;
        }
    }

    void insert_one(const database_type& db, const std::string& coll, const nlohmann::json& js)
    {
        database& cdb = get_db(db);

        std::lock_guard guard(cdb.get_lock(coll));

        ///so, it used to be that the db would not import the database if just inserting
        ///this was a huge issue for massively fragmented dbs like the msg db that might be enormous, but don't need to be read
        ///nowadays this is done by lmdb, and forcing an import for user dbs (which is the only consumer of this system)
        ///is necessary for db caps
        import_collection_nolock(db, coll);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        auto fdata = js;
        fdata[CID_STRING] = get_next_id();

        collection.push_back(fdata);

        flush(db, coll, fdata);
    }

    template<typename T>
    void for_each_match_nolock(const database_type& db, const std::string& coll, const nlohmann::json& selector, const T& t)
    {
        database& cdb = get_db(db);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        for(nlohmann::json& js : collection)
        {
            if(matches(js, selector))
            {
                if(t(js))
                    return;
            }
        }
    }

    ///ensure that update can never contain CID_STRING
    void update_one(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& update)
    {
        database& cdb = get_db(db);

        std::lock_guard guard(cdb.get_lock(coll));

        import_collection_nolock(db, coll);

        for_each_match_nolock(db, coll, selector, [&](nlohmann::json& js)
        {
            updater(js, update);

            flush(db, coll, js);
            return true;
        });
    }

    void update_many(const database_type& db, const std::string& coll, const nlohmann::json& selector, const nlohmann::json& update)
    {
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
        database& cdb = get_db(db);

        std::lock_guard guard(cdb.get_lock(coll));

        import_collection_nolock(db, coll);

        std::vector<nlohmann::json>& collection = cdb.get_collection(coll);

        for(auto& js : collection)
        {
            if(matches(js, selector))
            {
                disk_erase(db, coll, js);
            }
        }

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
    db_storage_backend::run_tests();

    db_storage& store = get_db_storage();

    #ifdef __WIN32__
    mkdir(ROOT_STORE);
    #else
    mkdir(ROOT_STORE, 0777);
    #endif // __WIN32__

    std::string root_file = ROOT_FILE;

    std::string resulting_data;

    try
    {
        resulting_data = read_file(root_file);
    }
    catch(...){}

    std::cout << "RDATA " << resulting_data << std::endl;

    if(resulting_data.size() == 0)
    {
        //throw std::runtime_error("Yeah we're past this point! file explosion");

        store.atomic_write(root_file, std::to_string(0));

        std::cout << "writing to " << root_file << std::endl;
        //write_all(root_file, std::to_string(0));
    }
    else
    {
        store.global_id = atoll(resulting_data.c_str());
        store.reserved_global_id = store.global_id;
    }

    if(!file_exists(root_file))
    {
        printf("Failed root file\n");
        exit(4);
    }

    for(int idx=0; idx < (int)mongo_database_type::MONGO_COUNT; idx++)
    {
        store.all_data[(int)mongo_databases[idx]->last_db_type];
    }
}

void db_storage_backend::run_tests()
{
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

void db_storage_backend::change_collection_unsafe(const std::string& coll, bool force_change)
{
    if(is_fixed && !force_change)
    {
        std::cout << "warning, collection should not be changed" << std::endl;
        return;
    }

    if(coll == collection && !force_change)
        return;

    collection = coll;
}

db_storage_backend::db_storage_backend(database_type _database, bool _is_fixed)
{
    if(_database != (int)mongo_database_type::USER_ACCESSIBLE)
        throw std::runtime_error("Somehow used the old db system");

    database = _database;
    is_fixed = _is_fixed;
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

lock_type_t& db_storage_backend::get_lock_for()
{
    return get_db_storage().get_db(database).get_lock(collection);
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
