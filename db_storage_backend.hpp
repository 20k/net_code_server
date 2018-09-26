#ifndef DB_STORAGE_BACKEND_HPP_INCLUDED
#define DB_STORAGE_BACKEND_HPP_INCLUDED

#include <string>
#include <json/json.hpp>
#include "mongoc_fwd.hpp"

struct mongo_context;

using database_type = std::string;

void init_db_storage_backend();

struct db_storage_backend
{
    static void run_tests();

    mongo_context* ctx = nullptr;

    database_type database;
    std::string collection;

    static
    bool contains_banned_query(const nlohmann::json& js);

    void change_collection_unsafe(const std::string& coll, bool force_change = false);

    /*void insert_bson_1(const std::string& script_host, bson_t* bs) const;
    void insert_json_1(const std::string& script_host, const std::string& json) const;

    std::string update_bson_many(const std::string& script_host, bson_t* selector, bson_t* update) const;
    std::string update_json_many(const std::string& script_host, const std::string& selector, const std::string& update) const;

    std::string update_bson_one(bson_t* selector, bson_t* update) const;
    std::string update_json_one(const std::string& selector, const std::string& update) const;

    std::vector<std::string> find_bson(const std::string& script_host, bson_t* bs, bson_t* ps);
    std::vector<std::string> find_json(const std::string& script_host, const std::string& json, const std::string& proj);

    void remove_bson(const std::string& script_host, bson_t* bs);
    void remove_json(const std::string& script_host, const std::string& json);*/

    void insert_one(const nlohmann::json& json);
    void update_one(const nlohmann::json& selector, const nlohmann::json& update);
    void update_many(const nlohmann::json& selector, const nlohmann::json& update);
    std::vector<nlohmann::json> find_many(const nlohmann::json& selector, const nlohmann::json& projector);
    void remove_many(const nlohmann::json& selector);

    db_storage_backend(mongo_context* fctx);
};

#endif // DB_STORAGE_BACKEND_HPP_INCLUDED
