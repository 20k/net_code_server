#ifndef COMMAND_HANDLER_FIBER_BACKEND_HPP_INCLUDED
#define COMMAND_HANDLER_FIBER_BACKEND_HPP_INCLUDED

#include <vector>
#include <functional>
#include "safe_thread.hpp"

void boot_fiber_manager();

bool is_thread_fiber();

struct fiber_queue
{
    std::vector<std::function<void()>> q;
    lock_type_t lock;

    template<typename T>
    void add(T t)
    {
        std::lock_guard guard(lock);
        q.push_back(t);
    }
};

inline
fiber_queue& get_global_fiber_queue()
{
    static fiber_queue q;
    return q;
}

#endif // COMMAND_HANDLER_FIBER_BACKEND_HPP_INCLUDED
