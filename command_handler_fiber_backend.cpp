#include "command_handler_fiber_backend.hpp"

#include <boost/fiber/all.hpp>
#include <boost/fiber/algo/round_robin.hpp>
#include <SFML/System/Clock.hpp>
#include "safe_thread.hpp"
#include <queue>
#include <SFML/System/Sleep.hpp>
#include <atomic>
#include <deque>

#ifndef __WIN32__
#include <pthread.h>
#include <sched.h>
#endif // __WIN32__

#define SCRIPT_THREADS 3

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
    std::deque<boost::fibers::context*> q;
    std::atomic_int approx_queue_size = 0;
    boost::fibers::fiber::id dispatcher_id;
    //std::mutex lock;
};

void fiber_sleep(double time_ms)
{
    int ms = (int)time_ms;

    if(is_thread_fiber())
    {
        boost::this_fiber::sleep_for(std::chrono::milliseconds(ms));
    }
    else
    {
        sthread::this_sleep(ms);
    }
}

void fiber_yield()
{
    if(is_thread_fiber())
    {
        boost::this_fiber::yield();
    }
}

//std::array<scheduler_data, HARDWARE_THREADS> global_data;

struct custom_scheduler : boost::fibers::algo::algorithm
{
    int my_id = 0;
    scheduler_data& dat;

    custom_scheduler(int id, scheduler_data& _dat) : my_id(id), dat(_dat)
    {

    }

    void awakened(boost::fibers::context* f) noexcept override
    {
        //std::lock_guard guard(dat.lock);

        /*if(f->get_id() == dat.dispatcher_id)
            dat.q.push_front(f);
        else*/
            dat.q.push_back(f);

        dat.approx_queue_size = dat.approx_queue_size + 1;
    }

    boost::fibers::context* pick_next() noexcept override
    {
        //std::lock_guard guard(dat.lock);

        if(dat.q.size() == 0)
            return nullptr;

        boost::fibers::context* next = dat.q.front();
        dat.q.pop_front();
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
        //#ifndef EXTERN_IP
        sf::sleep(sf::milliseconds(1));
        //#endif // EXTERN_IP
    }

    void notify() noexcept override
    {

    }
};

template<int HARDWARE_THREADS>
void worker_thread(int id, std::array<scheduler_data, HARDWARE_THREADS>* pothers, bool high_priority, fiber_queue& fqueue)
{
    std::array<scheduler_data, HARDWARE_THREADS>& others = *pothers;

    #ifdef USE_FIBERS
    boost::fibers::use_scheduling_algorithm<custom_scheduler>(id, others[id]);
    is_fiber = 1;

    others[id].dispatcher_id = boost::this_fiber::get_id();

    std::cout << "WORKER FIBER ID " << others[id].dispatcher_id << std::endl;

    fiber_queue& queue = fqueue;

    thread_priority_handler tp;

    if(high_priority)
        tp.enable();

    printf("Boot fiber worker %i\n", id);

    #ifndef __WIN32__

    if(high_priority)
    {
        //#define MY_SCHED SCHED_FIFO
        #define MY_SCHED SCHED_RR

        sched_param sp = { .sched_priority = 50 };
        int ret = sched_setscheduler(0, MY_SCHED, &sp);

        if (ret == -1)
        {
            printf("sched_setscheduler");
            return;
        }

        sched_param params;
        params.sched_priority = sched_get_priority_max(MY_SCHED);

        pthread_t this_thread = pthread_self();

        std::cout << "Trying to set thread realtime prio = " << params.sched_priority << std::endl;

        ret = pthread_setschedparam(this_thread, MY_SCHED, &params);

        if (ret != 0)
        {
            // Print the error
            std::cout << "Unsuccessful in setting thread realtime prio" << std::endl;
            return;
        }
    }
    #endif // __WIN32__

    while(1)
    {
        bool found_work = false;

        int my_size = others[id].q.size();
        bool small = true;

        if(my_size > 0)
        {
            for(int i=0; i < HARDWARE_THREADS; i++)
            {
                if(i == id)
                    continue;

                ///significant difference in thread load
                if(others[i].approx_queue_size < my_size - 3)
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
                found_work = true;
            }
        }

        if(!found_work)
            boost::this_fiber::sleep_for(std::chrono::milliseconds(1));


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

/*struct fiber_overload_data
{
    std::array<scheduler_data, SCRIPT_THREADS>* data;
};

static fiber_overload_data global_data;

float fiber_overload_factor()
{
    if(global_data.data == nullptr)
        return 1;

    float factor = 0;

    for(int i=0; i < SCRIPT_THREADS; i++)
    {
        factor += (*global_data.data)[i].approx_queue_size / 2;
    }

    factor /= SCRIPT_THREADS;

    return std::max(factor, 1.f);
}*/

std::atomic_int& get_scripts_running()
{
    static std::atomic_int scripts_running;

    return scripts_running;
}

float fiber_overload_factor()
{
    float factor = (get_scripts_running() / 2.f) / SCRIPT_THREADS;

    //float factor = scripts_running / SCRIPT_THREADS

    return std::max(factor, 1.f);
}

void fiber_work::notify(int cost)
{
    get_scripts_running() += cost;
}

void fiber_work::unnotify(int cost)
{
    get_scripts_running() -= cost;
}

void boot_fiber_manager()
{
    printf("FIBER START\n");

    #ifdef USE_FIBERS
    printf("Boot Script Scheduler\n");

    std::array<scheduler_data, SCRIPT_THREADS>* script_workers = new std::array<scheduler_data, 3>;

    //global_data.data = script_workers;

    for(int i=0; i < SCRIPT_THREADS; i++)
    {
        std::thread(worker_thread<SCRIPT_THREADS>, i, script_workers, true, std::ref(get_global_fiber_queue())).detach();
    }

    printf("Boot Server Scheduler");

    std::array<scheduler_data, 1>* server_workers = new std::array<scheduler_data, 1>;

    for(int i=0; i < 1; i++)
    {
        std::thread(worker_thread<1>, i, server_workers, false, std::ref(get_noncritical_fiber_queue())).detach();
    }

    printf("Finished booting fibers\n");

    #endif // USE_FIBERS
}
