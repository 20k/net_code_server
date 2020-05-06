#include "command_handler_fiber_backend.hpp"

#include <boost/fiber/all.hpp>
#include <boost/fiber/algo/round_robin.hpp>
#include <SFML/System/Clock.hpp>

void worker_thread()
{
    boost::fibers::use_scheduling_algorithm<boost::fibers::algo::round_robin>();

    while(1)
    {
        boost::this_fiber::yield();
        boost::this_fiber::sleep_for(std::chrono::milliseconds(1));
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
