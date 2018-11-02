#ifndef SAFE_THREAD_HPP_INCLUDED
#define SAFE_THREAD_HPP_INCLUDED

#include <thread>
#include <type_traits>
#include "stacktrace.hpp"
#include <thread>
//#include "thread_debugger.hpp"
#include <iostream>

struct sthread
{
    std::thread thrd;

    template<typename T, typename... U>
    sthread(T&& t, U&&... u) : thrd([](auto t, auto... u)
                                    {
                                        //pthread_t thread = pthread_self();

                                        //HANDLE h = pthread_gethandle(thread);

                                        stack_on_start();

                                        //get_thread_registration().add(h);

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

                                        //get_thread_registration().rem(h);

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

    void* winapi_handle()
    {
        pthread_t thread = native_handle();
        void* native_handle = pthread_gethandle(thread);

        return native_handle;
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


#endif // SAFE_THREAD_HPP_INCLUDED
