#include "non_user_task_thread.hpp"

#include <chrono>
#include "mongo.hpp"
#include <thread>
#include <windows.h>
#include "shared_data.hpp"
#include "privileged_core_scripts.hpp"
#include "rate_limiting.hpp"
#include "command_handler.hpp"

void bot_thread()
{
    while(1)
    {
        Sleep(1000);

        ///milliseconds
        size_t current_time = get_wall_time();

        std::vector<mongo_requester> found;

        {
            mongo_lock_proxy all_users = get_global_mongo_global_properties_context(-2);

            mongo_requester to_find;
            to_find.exists_check["users"] = 1;

            found = to_find.fetch_from_db(all_users);
        }

        for(mongo_requester& found_auth : found)
        {
            std::vector<std::string> users = str_to_array(found_auth.get_prop("users"));

            for(std::string& username : users)
            {
                user found_user;

                {
                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(-2);

                    found_user.load_from_db(mongo_ctx, username);
                }

                //std::cout << "checking user " << found_user.name << std::endl;

                std::vector<std::string> loaded = found_user.all_loaded_items();

                for(std::string& item_id : loaded)
                {
                    item next_item;

                    {
                        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                        next_item.load_from_db(mongo_ctx, item_id);
                    }

                    std::string type = next_item.get_prop("item_type");

                    if(type != std::to_string(item_types::AUTO_SCRIPT_RUNNER))
                        continue;

                    //std::cout << "of type bot brain" << std::endl;

                    size_t found_time_ms = next_item.get_prop_as_long("last_run");
                    double run_s = next_item.get_prop_as_double("run_every_s");

                    size_t next_time = found_time_ms + run_s * 1000;

                    //std::cout << "cur " << current_time << " next " << next_time << " run " << run_s << std::endl;

                    if(current_time >= next_time)
                    {
                        ///WE'RE A VALID BOT BRAIN SCRIPT
                        ///RUN AND THEN BREAK

                        next_item.set_prop("last_run", current_time);

                        {
                            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);
                            next_item.overwrite_in_db(mongo_ctx);
                        }

                        std::cout << "running script autorun " << found_user.name << std::endl;

                        throwaway_user_thread(found_user.name, "#" + found_user.name + ".autorun()");

                        Sleep(100);

                        break;
                    }
                }
            }
        }
    }
}

void run_non_user_tasks()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    size_t real_time = duration.count();

    while(1)
    {
        Sleep(500);

        get_global_rate_limit()->donate_time_budget(0.5f);

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
            std::lock_guard guard(store->lock);

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
    std::thread(run_non_user_tasks).detach();
    std::thread(bot_thread).detach();
}
