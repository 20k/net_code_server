#ifndef REOCCURRING_TASK_HANDLER_HPP_INCLUDED
#define REOCCURRING_TASK_HANDLER_HPP_INCLUDED

#include <functional>
#include <vector>
#include <mutex>

struct reoccuring_task
{
    size_t time_between_tasks = 0;
    size_t last_time = 0;
    std::function<void()> func;
};

struct reoccuring_task_handler
{
    std::mutex mut;
    std::vector<reoccuring_task*> tasks;

    reoccuring_task_handler();

    void register_task(const std::function<void()>& func, size_t time_between_tasks_s);
};

reoccuring_task_handler& get_global_task_handler();

#endif // REOCCURRING_TASK_HANDLER_HPP_INCLUDED
