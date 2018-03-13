#ifndef USER_HPP_INCLUDED
#define USER_HPP_INCLUDED

#include "mongo.hpp"
#include "script_util.hpp"
#include "item.hpp"

///how do handle upgrades
///vector of ids?

#define MAX_ITEMS 128

///ok. Need to fetch users out of the db
struct user
{
    std::string name;
    double cash = 0;
    std::string auth;
    int32_t last_message_uid = 0;
    std::string upgr_idx;
    std::string loaded_upgr_idx;
    std::string user_port;

    bool valid = false;

    void overwrite_user_in_db(mongo_lock_proxy& ctx);
    bool exists(mongo_lock_proxy& ctx, const std::string& name_);
    bool load_from_db(mongo_lock_proxy& ctx, const std::string& name_);
    bool construct_new_user(mongo_lock_proxy& ctx, const std::string& name_, const std::string& auth, int last_message_uid_);

    std::map<std::string, double> get_properties_from_loaded_items(mongo_lock_proxy& ctx);

    std::map<std::string, double> get_total_user_properties(mongo_lock_proxy& ctx);

    bool has_loaded_item(const std::string& id);
    bool load_item(const std::string& id);
    void unload_item(const std::string& id);
    std::vector<std::string> all_loaded_items();

    //std::string get_loaded_callable_scriptname_source(mongo_lock_proxy& ctx, const std::string& full_name);

    item get_loaded_callable_scriptname_item(mongo_lock_proxy& ctx, const std::string& full_name);

    std::string index_to_item(int index);
    int item_to_index(const std::string& item);

    void append_item(const std::string& id);
    bool has_item(const std::string& id);
    void remove_item(const std::string& id);

    int num_items();

    int find_num_scripts(mongo_lock_proxy& ctx);
    int find_num_public_scripts(mongo_lock_proxy& ctx);
};

#endif // USER_HPP_INCLUDED
