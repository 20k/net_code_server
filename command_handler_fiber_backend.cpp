#include "command_handler_fiber_backend.hpp"

#include <boost/fiber/all.hpp>
#include <boost/fiber/algo/round_robin.hpp>
#include <SFML/System/Clock.hpp>
#include "safe_thread.hpp"
#include <queue>
#include <SFML/System/Sleep.hpp>
#include <atomic>

//tls_variable<int, 0> is_fiber;

#define HARDWARE_THREADS 3

thread_local int is_fiber;

bool is_thread_fiber()
{
    #ifdef USE_FIBERS
    return is_fiber;
    #else
    return false;
    #endif
}

struct scheduler_data
{
    std::queue<boost::fibers::context*> q;
    std::atomic_int approx_queue_size = 0;
    //std::mutex lock;
};

std::array<scheduler_data, HARDWARE_THREADS> global_data;

struct custom_scheduler : boost::fibers::algo::algorithm
{
    int my_id = 0;
    scheduler_data& dat;

    custom_scheduler(int id) : my_id(id), dat(global_data[my_id])
    {

    }

    void awakened(boost::fibers::context* f) noexcept override
    {
        //std::lock_guard guard(dat.lock);

        dat.q.push(f);
        dat.approx_queue_size = dat.approx_queue_size + 1;
    }

    boost::fibers::context* pick_next() noexcept override
    {
        //std::lock_guard guard(dat.lock);

        if(dat.q.size() == 0)
            return nullptr;

        boost::fibers::context* next = dat.q.front();
        dat.q.pop();
        dat.approx_queue_size = dat.approx_queue_size - 1;

        return next;
    }

    bool has_ready_fibers() const noexcept override
    {
        //std::lock_guard guard(dat.lock);

        return dat.q.size() > 0;
    }

    void suspend_until(std::chrono::steady_clock::time_point const&) noexcept override
    {
        sf::sleep(sf::milliseconds(1));
    }

    void notify() noexcept override
    {

    }
};

void worker_thread(int id)
{
    #ifdef USE_FIBERS
    boost::fibers::use_scheduling_algorithm<custom_scheduler>(id);
    is_fiber = 1;

    fiber_queue& queue = get_global_fiber_queue();

    thread_priority_handler tp;
    tp.enable();

    while(1)
    {
        //boost::this_fiber::yield();
        boost::this_fiber::sleep_for(std::chrono::milliseconds(100));

        int my_size = global_data[id].q.size();
        bool small = true;

        if(my_size > 0)
        {
            for(int i=0; i < HARDWARE_THREADS; i++)
            {
                if(i == id)
                    continue;

                ///significant difference in thread load
                if(global_data[i].approx_queue_size < my_size - 3)
                    small = false;
            }
        }

        if(small)
        {
            safe_lock_guard guard(queue.lock);

            if(queue.q.size() > 0)
            {
                boost::fibers::fiber([](auto in)
                {
                    try
                    {
                        in();
                    }
                    catch(std::exception& e)
                    {
                        std::cout << "Caught exception in fibre manager" << e.what() << std::endl;
                    }
                }, queue.q[0]).detach();

                queue.q.erase(queue.q.begin());
                continue;
            }
        }

        ///work stealing doesn't work with quickjs
        /*{
            //std::lock_guard guard(global_data.lock);

            for(int i=0; i < HARDWARE_THREADS; i++)
            {
                if(i == id)
                    continue;

                ///I know nobody can be accessing my lock
                std::lock_guard guard(global_data[i].lock);

                if(global_data[i].q.size() > 2)
                {
                    boost::fibers::context* ctx = global_data[i].q.front();
                    global_data[i].q.pop();
                    global_data[id].q.push(ctx);
                }
            }
        }*/
    }
    #endif // USE_FIBERS
}

void boot_fiber_manager()
{
    #ifdef USE_FIBERS
    printf("Boot?\n");

    for(int i=0; i < HARDWARE_THREADS; i++)
    {
        std::thread(worker_thread, i).detach();
    }
    #endif // USE_FIBERS
}
