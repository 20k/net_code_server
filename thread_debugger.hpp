#ifndef THREAD_DEBUGGER_HPP_INCLUDED
#define THREAD_DEBUGGER_HPP_INCLUDED

#include <string>
#include <windows.h>
#include <vector>
#include <mutex>

struct safe_thread;

std::string get_all_thread_stacktraces();

struct thread_registration
{
    std::vector<HANDLE> h_threads;

    std::mutex thread_lock;

    void add(HANDLE h)
    {
        std::lock_guard guard(thread_lock);

        h_threads.push_back(h);
    }

    void rem(HANDLE h)
    {
        std::lock_guard guard(thread_lock);

        for(int i=0; i < (int)h_threads.size(); i++)
        {
            if(h_threads[i] == h)
            {
                h_threads.erase(h_threads.begin() + i);
                i--;
                continue;
            }
        }
    }

    std::vector<HANDLE> fetch()
    {
        std::lock_guard guard(thread_lock);

        return h_threads;
    }
};

inline
thread_registration& get_thread_registration()
{
    static thread_registration r;

    return r;
}

#endif // THREAD_DEBUGGER_HPP_INCLUDED
