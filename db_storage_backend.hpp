#ifndef DB_STORAGE_BACKEND_HPP_INCLUDED
#define DB_STORAGE_BACKEND_HPP_INCLUDED

#include <string>
#include <nlohmann/json.hpp>
#include "mongoc_fwd.hpp"
#include <mutex>
#include "stacktrace.hpp"
#include <iostream>
#include <SFML/System/Clock.hpp>

#define CID_STRING "_cid"

struct mongo_context;

using database_type = int32_t;

void init_db_storage_backend();

struct safe_mutex
{
    std::mutex mutex;

    void lock()
    {
        /*sf::Clock clk;

        while(!mutex.try_lock())
        {
            if(clk.getElapsedTime().asMilliseconds() > 5000)
            {
                std::cout << get_stacktrace() << std::endl;
            }
        }*/

        mutex.lock();
    }

    void unlock()
    {
        mutex.unlock();
    }
};

struct db_storage_backend
{
    static void run_tests();

    mongo_context* ctx = nullptr;

    database_type database;
    std::string collection;

    static
    void make_backup(const std::string& to_where);

    void change_collection_unsafe(const std::string& coll, bool force_change = false);

    void insert_one(const nlohmann::json& json);
    void update_one(const nlohmann::json& selector, const nlohmann::json& update);
    void update_many(const nlohmann::json& selector, const nlohmann::json& update);
    std::vector<nlohmann::json> find_many(const nlohmann::json& selector, const nlohmann::json& options);
    void remove_many(const nlohmann::json& selector);

    std::mutex& get_lock_for();
    std::vector<nlohmann::json>& get_db_data_nolock_import();
    void flush(const nlohmann::json& data);
    void disk_erase(const nlohmann::json& data);

    static
    size_t get_unique_id();

    db_storage_backend(mongo_context* fctx);
};

void remove_mongo_id(nlohmann::json& in);

#endif // DB_STORAGE_BACKEND_HPP_INCLUDED
