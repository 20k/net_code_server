#include "non_user_task_thread.hpp"

#include <chrono>
#include "mongo.hpp"
#include <thread>
#include <windows.h>

void run_non_user_tasks()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    size_t real_time = duration.count();

    while(1)
    {
        Sleep(250);

        auto next_now = std::chrono::high_resolution_clock::now();
        auto next_duration = now.time_since_epoch();
        size_t next_real_time = duration.count();

        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channels_context(-2);

        mongo_requester request;
        request.gt_than["time_ms"] = stringify_hack(real_time);
        request.lt_than["time_ms"] = stringify_hack(next_real_time);

        std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

        real_time = next_real_time;
    }
}

void start_non_user_task_thread()
{
    std::thread(run_non_user_tasks).detach();
}
