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
        Sleep(1000);

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

        std::string to_send = prettify_chat_strings(found);

        global_shared_data* store = fetch_global_shared_data();

        //std::cout << found.size() << std::endl;

        if(to_send != "")
        {
            std::lock_guard guard(store->lock);

            for(shared_data* data : store->data)
            {
                data->add_back_write(to_send);
            }
        }

        real_time = next_real_time;
    }
}

void start_non_user_task_thread()
{
    std::thread(run_non_user_tasks).detach();
}
