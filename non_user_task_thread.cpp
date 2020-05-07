#include "non_user_task_thread.hpp"

#include <chrono>
#include "mongo.hpp"
#include <thread>
#include "shared_data.hpp"
//#include "privileged_core_scripts.hpp"
#include "rate_limiting.hpp"
#include "command_handler.hpp"
#include <secret/npc_manager.hpp>
#include "safe_thread.hpp"
#include "unified_scripts.hpp"
#include "command_handler_fiber_backend.hpp"

///THESE ARE NOW INCORRECT DUE TO BAD AUTH USAGE

//#define ONE_TIME_MANHANDLE
#ifdef ONE_TIME_MANHANDLE
void manhandle_away_critical_users()
{
    std::set<std::string> banned;

    for(auto& i : privileged_args)
    {
        std::string script_name = i.first;

        std::string str = no_ss_split(script_name, ".")[0];

        banned.insert(str);
    }

    user i20k_auth;

    std::string auth;

    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

        i20k_auth.load_from_db(ctx, "i20k");

        auth = i20k_auth.auth;
    }

    auto steal_from = [&](user& usr)
    {
        if(banned.find(usr.name) != banned.end())// && usr.auth != auth)
        {
            std::string old_auth = usr.auth;

            usr.auth = auth;

            {
                mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

                usr.overwrite_user_in_db(ctx);
            }

            mongo_lock_proxy auth_db = get_global_mongo_global_properties_context(-2);

            mongo_requester req;
            req.set_prop_bin("account_token", old_auth);

            auto found = req.fetch_from_db(auth_db);

            if(found.size() != 1)
                return;

            auto found_req = found[0];

            auto arr = (std::vector<std::string>)found_req.get_prop("users");

            for(int i=0; i < (int)arr.size(); i++)
            {
                if(arr[i] == usr.name)
                {
                    arr.erase(arr.begin() + i);
                    i--;
                    continue;
                }
            }

            found_req.set_prop("users", arr);

            req.update_in_db_if_exact(auth_db, found_req);

            std::cout << "stole user " << usr.name << std::endl;
        }
    };

    for_each_user(steal_from);
}

void manhandle_thread()
{
    while(1)
    {
        manhandle_away_critical_users();

        fiber_sleep(10000);
    }
}
#endif // ONE_TIME_MANHANDLE

extern size_t get_wall_time();

void bot_thread()
{
    while(1)
    {
        fiber_sleep(1000);

        ///milliseconds
        size_t current_time = get_wall_time();

        auto check_autorun = [&](user& found_user)
        {
            fiber_sleep(5);

            {
                std::string err;

                if(!unified_script_loading(-2, found_user.name + ".autorun", err).valid)
                    return;
            }

            std::vector<std::string> loaded = found_user.all_loaded_items();

            for(std::string& item_id : loaded)
            {
                item next_item;

                {
                    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                    db_disk_load(mongo_ctx, next_item, item_id);
                }

                int type = next_item.get_int("item_type");

                if(type != item_types::AUTO_SCRIPT_RUNNER)
                    continue;

                //std::cout << "of type bot brain" << std::endl;

                size_t found_time_ms = next_item.get_size_t("last_run");
                double run_s = next_item.get_double("run_every_s");

                size_t next_time = found_time_ms + run_s * 1000;

                //std::cout << "cur " << current_time << " next " << next_time << " run " << run_s << std::endl;

                if(current_time >= next_time)
                {
                    ///WE'RE A VALID BOT BRAIN SCRIPT
                    ///RUN AND THEN BREAK

                    next_item.set_as("last_run", current_time);

                    {
                        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);
                        db_disk_overwrite(mongo_ctx, next_item);
                    }

                    std::cout << "running script autorun " << found_user.name << std::endl;

                    throwaway_user_thread(found_user.name, "#" + found_user.name + ".autorun()");

                    fiber_sleep(100);

                    break;
                }
            }
        };

        for_each_user(check_autorun);

        for_each_npc(check_autorun);
    }
}

void run_non_user_tasks()
{
    //auto now = std::chrono::high_resolution_clock::now();
    //auto duration = now.time_since_epoch();
    //size_t real_time = duration.count();

    while(1)
    {
        fiber_sleep(500);

        get_global_rate_limit()->donate_time_budget(0.5f);

        /*global_shared_data* store = fetch_global_shared_data();

        {
            mongo_lock_proxy ctx = get_global_mongo_pending _notifs_context(-2);
            safe_lock_guard guard(store->lock);

            for(shared_data* data : store->data)
            {

            }
        }*/

        #if 0
        auto next_now = std::chrono::high_resolution_clock::now();
        auto next_duration = next_now.time_since_epoch();
        size_t next_real_time = next_duration.count();

        std::vector<mongo_requester> found;

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channels_context(-2);

            mongo_requester request;
            request.gt_than["time_ms"] = stringify_hack(real_time);
            request.lt_than["time_ms"] = stringify_hack(next_real_time);

            found = request.fetch_from_db(mongo_ctx);
        }

        //std::string to_send = prettify_chat_strings(found);

        std::map<std::string, std::vector<mongo_requester>> channel_map;
        std::map<std::string, std::string> channel_to_string;

        for(auto& i : found)
        {
            channel_map[i.get_prop("channel")].push_back(i);
        }

        for(auto& i : channel_map)
        {
            std::sort(i.second.begin(), i.second.end(), [](auto& i1, auto& i2){return i1.get_prop("uid") > i2.get_prop("uid");});
        }

        for(auto& i : channel_map)
        {
            channel_to_string[i.first] = prettify_chat_strings(i.second);
        }

        global_shared_data* store = fetch_global_shared_data();

        //std::cout << found.size() << std::endl;

        if(channel_to_string.size() != 0)
        {
            safe_lock_guard guard(store->lock);

            for(shared_data* data : store->data)
            {
                for(auto& cdata : channel_to_string)
                {
                    std::string to_send = "chat_api " + cdata.first + " " + cdata.second;

                    data->add_back_write(to_send);
                }

                //data->add_back_write(to_send);
            }
        }

        real_time = next_real_time;
        #endif // 0
    }
}

void start_non_user_task_thread()
{
    get_noncritical_fiber_queue().add(run_non_user_tasks);
    get_noncritical_fiber_queue().add(bot_thread);
    #ifdef ONE_TIME_MANHANDLE
    get_noncritical_fiber_queue().add(manhandle_away_critical_users);
    #endif // ONE_TIME_MANHANDLE

    //fix_auth_errors();
}
