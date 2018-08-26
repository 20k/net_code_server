#ifndef USER_HPP_INCLUDED
#define USER_HPP_INCLUDED

#include "mongo.hpp"
#include "script_util.hpp"
#include "item.hpp"

#include <secret/structure.hpp>
#include "timestamped_position.hpp"

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
    std::vector<std::string> users_i_have_access_to;

    std::string joined_channels;

    space_pos_t pos;

    bool has_local_pos = false;

    void overwrite_user_in_db(mongo_lock_proxy& ctx);
    bool exists(mongo_lock_proxy& ctx, const std::string& name_);
    bool load_from_db(mongo_lock_proxy& ctx, const std::string& name_);
    bool construct_new_user(mongo_lock_proxy& ctx, const std::string& name_, const std::string& auth);

    static void delete_from_cache(const std::string& name_);

    std::map<std::string, double> get_properties_from_loaded_items(mongo_lock_proxy& ctx);

    std::map<std::string, double> get_total_user_properties(int thread_id);

    bool has_loaded_item(const std::string& id);
    bool load_item(const std::string& id);
    void unload_item(const std::string& id);
    std::vector<std::string> all_loaded_items();

    //std::string get_loaded_callable_scriptname_source(mongo_lock_proxy& ctx, const std::string& full_name);

    item get_loaded_callable_scriptname_item(mongo_lock_proxy& ctx, const std::string& full_name);
    std::vector<item> get_all_items(mongo_lock_proxy& ctx);
    std::vector<std::string> get_all_items();

    std::string index_to_item(int index);
    int item_to_index(const std::string& item);

    void append_item(const std::string& id);
    bool has_item(const std::string& id);
    void remove_item(const std::string& id);

    void clear_items(int thread_id);

    int num_items();

    void cleanup_call_stack(int thread_id);
    std::vector<std::string> get_call_stack();

    ///manually injects self
    std::vector<std::string> get_allowed_users();
    std::vector<std::string> get_users_i_have_access_to();

    bool is_allowed_user(const std::string& usr); ///returns true for self
    void add_allowed_user(const std::string& usr, mongo_lock_proxy& ctx);
    void remove_allowed_user(const std::string& usr, mongo_lock_proxy& ctx); ///does nothing on self

    int find_num_scripts(mongo_lock_proxy& ctx);
    int find_num_public_scripts(mongo_lock_proxy& ctx);

    bool is_npc() const;

    int get_default_network_links(int thread_id);

    std::string fetch_sector();

    timestamp_move_queue get_timestamp_queue();
    space_pos_t get_local_pos() const;
    timestamped_position get_final_pos() const;
    void set_local_pos(space_pos_t pos, int replace_item_at = -1);
    void add_position_target(space_pos_t pos, size_t time_delta_ms, std::string notif_on_finish = "");
    void add_activate_target(size_t current_time, const std::string& destination_sys);
    void reset_internal_queue();

    ///currently used for move notifs
    void pump_notifications(int lock_id);

    virtual ~user(){}

    static void launch_pump_events_thread();

protected:
    //space_pos_t local_pos;
    timestamp_move_queue move_queue;
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

                if(!usr.load_from_db(ctx, usrname))
                    continue;
            }

            t(usr);
        }
    }
}

template<typename T>
inline
void for_each_auth(const T& t)
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
        t(i);
    }
}

std::vector<user> load_users(const std::vector<std::string>& names, int lock_id);
std::vector<user> load_users_nolock(const std::vector<std::string>& names);

template<typename T>
inline
std::vector<user> filter_users(const std::vector<user>& users, int lock_id, const T& t)
{
    std::vector<user> ret;

    for(auto& i : users)
    {
        if(t(i))
        {
            ret.push_back(i);
        }
    }

    return ret;
}

#endif // USER_HPP_INCLUDED
