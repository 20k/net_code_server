#ifndef SAFE_THREAD_HPP_INCLUDED
#define SAFE_THREAD_HPP_INCLUDED

#include <thread>
#include <type_traits>
#include "stacktrace.hpp"
#include <thread>
//#include "thread_debugger.hpp"
#include <iostream>
#include <mutex>
#include <sys/time.h>

#ifdef USE_FIBERS
#include <boost/fiber/mutex.hpp>
#endif // USE_FIBERS

struct sthread
{
    std::thread thrd;

    template<typename T, typename... U>
    sthread(T&& t, U&&... u) : thrd([](auto t, auto... u)
                                    {
                                        stack_on_start();
                                        //setitimer()


                                        try{
                                            t(std::forward<U>(u)...);
                                        }
                                        catch(const std::exception& ex)
                                        {
                                            std::cout << "caught termination exception from thread " << ex.what() << std::endl;
                                            std::cout << "stack " << get_stacktrace() << std::endl;
                                        }
                                        catch(...)
                                        {
                                            std::cout << "caught termination exception from thread" << std::endl;
                                            std::cout << "stack " << get_stacktrace() << std::endl;
                                        }

                                    }, std::forward<T>(t), std::forward<U>(u)...)
    {

    }

    void join()
    {
        thrd.join();
    }

    void detach()
    {
        thrd.detach();
    }

    auto native_handle()
    {
        return thrd.native_handle();
    }

    static void this_yield();

    static void low_yield();

    static void this_sleep(int milliseconds);

    /*static void increase_priority()
    {
        SetThreadPriority(pthread_gethandle(pthread_self()), THREAD_PRIORITY_HIGHEST);
    }

    static void normal_priority()
    {
        SetThreadPriority(pthread_gethandle(pthread_self()), THREAD_PRIORITY_NORMAL);
    }

    static void increase_priority(std::thread& t)
    {
        pthread_t thread = t.native_handle();
        void* native_handle = pthread_gethandle(thread);

        SetThreadPriority(native_handle, THREAD_PRIORITY_HIGHEST);
    }

    static void normal_priority(std::thread& t)
    {
        pthread_t thread = t.native_handle();
        void* native_handle = pthread_gethandle(thread);

        SetThreadPriority(native_handle, THREAD_PRIORITY_NORMAL);
    }*/
};

struct thread_priority_handler
{
    void enable();
    ~thread_priority_handler();
};

struct lock_counter
{
    lock_counter();
    ~lock_counter();
};

template<typename T>
struct safe_lock_guard
{
    lock_counter cnt;
    std::lock_guard<T> guard;

    safe_lock_guard(T& t) : guard(t)
    {
        #ifdef DEADLOCK_DETECTION
        std::lock_guard<lock_type_t> g(mongo_context::thread_lock);

        mongo_context::thread_counter[std::this_thread::get_id()]++;

        if(mongo_context::thread_counter[std::this_thread::get_id()] > 1)
        {
            printf("bad guard\n");

            std::cout << boost::stacktrace::stacktrace() << std::endl;
        }
        #endif // DEADLOCK_DETECTION
    }

    ~safe_lock_guard()
    {
        #ifdef DEADLOCK_DETECTION
        std::lock_guard<lock_type_t> g(mongo_context::thread_lock);

        mongo_context::thread_counter[std::this_thread::get_id()]--;
        #endif // DEADLOCK_DETECTION
    }
};

struct safe_mutex
{
    #ifdef USE_FIBERS
    boost::fibers::mutex mutex;
    #else
    std::mutex mutex;
    #endif // USE_FIBERS

    void lock()
    {
        /*sf::Clock clk;

        while(!mutex.try_lock())
        {
            if(clk.getElapsedTime().asMilliseconds() > 5000)
            {
                std::cout << get_stacktrace() << std::endl;
            }
        }*/

        mutex.lock();
    }

    void unlock()
    {
        mutex.unlock();
    }
};

#ifdef USE_FIBERS

using lock_type_t = boost::fibers::mutex;
using shared_lock_type_t = boost::fibers::mutex;

template<typename T>
using shared_lock = std::unique_lock<T>;

template<typename T>
using unique_lock = std::unique_lock<T>;

#else

using lock_type_t = safe_mutex;
using shared_lock_type_t = shared_lock_type_t;

template<typename T>
using shared_lock = std::shared_lock<T>;

template<typename T>
using unique_lock = std::unique_lock<T>;

#endif // USE_FIBERS

#endif // SAFE_THREAD_HPP_INCLUDED
