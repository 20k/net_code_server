#include "reoccurring_task_handler.hpp"
#include "time.hpp"
#include "safe_thread.hpp"
#include "command_handler_fiber_backend.hpp"

void reoccuring_task_thread(reoccuring_task_handler& handler)
{
    while(1)
    {
        int idx = 0;
        size_t current_time_s = get_wall_time() / 1000;

        while(1)
        {
            fiber_yield();

            reoccuring_task* ctask = nullptr;

            {
                std::lock_guard guard(handler.mut);

                if(idx < (int)handler.tasks.size())
                    ctask = handler.tasks[idx];
                else
                    break;
            }

            if(ctask && ctask->last_time + ctask->time_between_tasks < current_time_s)
            {
                ctask->func();
                ctask->last_time = current_time_s;
            }

            idx++;
        }

        fiber_sleep(100);
    }
}

reoccuring_task_handler::reoccuring_task_handler()
{
    get_noncritical_fiber_queue().add(reoccuring_task_thread, std::ref(*this));
}

reoccuring_task_handler& get_global_task_handler()
{
    static reoccuring_task_handler ret;

    return ret;
}

void reoccuring_task_handler::register_task(const std::function<void()>& func, size_t time_between_tasks)
{
    reoccuring_task* task = new reoccuring_task;
    task->last_time = 0;
    //task->last_time = get_wall_time() / 1000;
    task->time_between_tasks = time_between_tasks;
    task->func = func;

    std::lock_guard guard(mut);

    tasks.push_back(task);
}
