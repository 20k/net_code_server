#ifndef SAFE_THREAD_HPP_INCLUDED
#define SAFE_THREAD_HPP_INCLUDED

#include <thread>
#include <type_traits>

struct sthread
{
    std::thread thrd;

    template<typename T, typename... U>
    sthread(T&& t, U&&... u) : thrd([&]()
                                    {
                                        try{
                                            t(std::forward<U>(u)...);
                                        }
                                        catch(...)
                                        {

                                        }
                                    })
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

    static void this_yield()
    {
        Sleep(0);

        #ifndef __WIN32__
        #error("doesn't work on linux");
        #endif // __WIN32__
    }

    static void low_yield()
    {
        std::this_thread::yield();
    }

    static void increase_priority()
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
    }
};


#endif // SAFE_THREAD_HPP_INCLUDED
