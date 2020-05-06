#include "command_handler_fiber_backend.hpp"

#include <boost/fiber/all.hpp>
#include <boost/fiber/algo/round_robin.hpp>
#include <SFML/System/Clock.hpp>
#include "safe_thread.hpp"

void worker_thread()
{
    boost::fibers::use_scheduling_algorithm<boost::fibers::algo::round_robin>();

    fiber_queue& queue = get_global_fiber_queue();

    while(1)
    {
        boost::this_fiber::yield();
        boost::this_fiber::sleep_for(std::chrono::milliseconds(1));

        safe_lock_guard guard(queue.lock);

        if(queue.q.size() > 0)
        {
            boost::fibers::fiber(queue.q[0]).detach();

            queue.q.erase(queue.q.begin());
        }
    }
}

void boot_fiber_manager()
{
    printf("Boot?\n");

    int hardware_threads = 3;

    for(int i=0; i < hardware_threads; i++)
    {
        std::thread(worker_thread).detach();
    }
}
