#include "command_handler_fiber_backend.hpp"

#include <boost/fiber/all.hpp>
#include <boost/fiber/algo/round_robin.hpp>
#include <SFML/System/Clock.hpp>
#include "safe_thread.hpp"
#include <queue>
#include <SFML/System/Sleep.hpp>

//tls_variable<int, 0> is_fiber;

thread_local int is_fiber;

bool is_thread_fiber()
{
    #ifdef USE_FIBERS
    return is_fiber;
    #else
    return false;
    #endif
}

struct custom_scheduler : boost::fibers::algo::algorithm
{
    std::queue<boost::fibers::context*> q;

    void awakened(boost::fibers::context* f) noexcept override
    {
        q.push(f);
    }

    boost::fibers::context* pick_next() noexcept override
    {
        if(q.size() == 0)
            return nullptr;

        boost::fibers::context* next = q.front();
        q.pop();

        return next;
    }

    bool has_ready_fibers() const noexcept override
    {
        return q.size() > 0;
    }

    void suspend_until(std::chrono::steady_clock::time_point const&) noexcept override
    {
        sf::sleep(sf::milliseconds(1));
    }

    void notify() noexcept override
    {

    }
};

void worker_thread()
{
    #ifdef USE_FIBERS
    boost::fibers::use_scheduling_algorithm<custom_scheduler>();
    is_fiber = 1;

    fiber_queue& queue = get_global_fiber_queue();

    thread_priority_handler tp;
    tp.enable();

    while(1)
    {
        //boost::this_fiber::yield();
        boost::this_fiber::sleep_for(std::chrono::milliseconds(100));

        safe_lock_guard guard(queue.lock);

        if(queue.q.size() > 0)
        {
            boost::fibers::fiber(queue.q[0]).detach();

            queue.q.erase(queue.q.begin());
        }
    }
    #endif // USE_FIBERS
}

void boot_fiber_manager()
{
    #ifdef USE_FIBERS
    printf("Boot?\n");

    int hardware_threads = 3;

    for(int i=0; i < hardware_threads; i++)
    {
        std::thread(worker_thread).detach();
    }
    #endif // USE_FIBERS
}
