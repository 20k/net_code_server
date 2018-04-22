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
    mongo_requester all_found_props;

    std::string name;
    double cash = 0;
    std::string auth;
    std::string upgr_idx;
    std::string loaded_upgr_idx;
    #ifdef USE_LOCS
    std::string user_port;
    #endif // USE_LOCS
    bool initial_connection_setup = false;

    ///stack of users, used for changing cli context
    std::vector<std::string> call_stack;
    std::vector<std::string> owner_list;

    bool valid = false;

    void overwrite_user_in_db(mongo_lock_proxy& ctx);
    bool exists(mongo_lock_proxy& ctx, const std::string& name_);
    bool load_from_db(mongo_lock_proxy& ctx, const std::string& name_);
    bool construct_new_user(mongo_lock_proxy& ctx, const std::string& name_, const std::string& auth);

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

    void clear_items(int thread_id);

    int num_items();

    std::vector<std::string> get_call_stack();

    ///manually injects self
    std::vector<std::string> get_allowed_users();

    bool is_allowed_user(const std::string& usr); ///returns true for self
    void add_allowed_user(const std::string& usr, mongo_lock_proxy& ctx);
    void remove_allowed_user(const std::string& usr, mongo_lock_proxy& ctx); ///does nothing on self

    int find_num_scripts(mongo_lock_proxy& ctx);
    int find_num_public_scripts(mongo_lock_proxy& ctx);

    virtual int get_default_network_links();

    virtual ~user(){}
};

template<typename T>
inline
void for_each_user(const T& t)
{
    std::vector<mongo_requester> all;

    {
        mongo_lock_proxy all_auth = get_global_mongo_global_properties_context(-2);

        mongo_requester request;

        request.exists_check["account_token"] = 1;

        all = request.fetch_from_db(all_auth);
    }

    for(auto& i : all)
    {
        auto users = str_to_array(i.get_prop("users"));

        for(std::string& usrname : users)
        {
            user usr;

            {
                mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

                usr.load_from_db(ctx, usrname);

                if(!usr.valid)
                    continue;
            }

            t(usr);
        }
    }
}

#endif // USER_HPP_INCLUDED
