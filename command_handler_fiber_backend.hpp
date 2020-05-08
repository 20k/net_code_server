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

    template<typename T, typename... U>
    void add(T&& t, U&&... u)
    {
        /*auto func = [&]()
        {
            t(std::forward<U>(u)...);
        };*/

        /*auto func = [t=std::move(t), =]()
        {
            t(u...);
        };*/

        std::lock_guard guard(lock);

        q.push_back(std::bind(t, std::forward<U>(u)...));
        //q.push_back(func);
    }
};

inline
fiber_queue& get_global_fiber_queue()
{
    static fiber_queue q;
    return q;
}

inline
fiber_queue& get_noncritical_fiber_queue()
{
    static fiber_queue q;
    return q;
}

void fiber_sleep(double time_ms);
void fiber_yield();

#endif // COMMAND_HANDLER_FIBER_BACKEND_HPP_INCLUDED
