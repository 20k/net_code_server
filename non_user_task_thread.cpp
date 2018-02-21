#include "non_user_task_thread.hpp"

#include <chrono>
#include "mongo.hpp"
#include <thread>
#include <windows.h>
#include "../crapmud_client/http_beast_client.hpp"
#include "privileged_core_scripts.hpp"

void run_non_user_tasks()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    size_t real_time = duration.count();

    while(1)
    {
        Sleep(500);

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
            std::sort(i.second.begin(), i.second.end(), [](auto& i1, auto& i2){return i1.get_prop("uid") >= i2.get_prop("uid");});
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
    }
}

void start_non_user_task_thread()
{
    std::thread(run_non_user_tasks).detach();
}
