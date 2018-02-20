#ifndef NON_USER_TASK_THREAD_HPP_INCLUDED
#define NON_USER_TASK_THREAD_HPP_INCLUDED

#include <vector>
#include <mutex>

struct shared_data;

///oh boy this is dangerous
struct global_shared_data
{
    std::vector<shared_data*> data;
    std::mutex lock;

    void add(shared_data* d)
    {
        ///c++17 to not specify type here
        ///now that i've remembered this is a feature, will probably
        ///litter the codebase
        std::lock_guard lk(lock);

        data.push_back(d);
    }
};

inline
global_shared_data* fetch_global_shared_data()
{
    static global_shared_data* dat = new global_shared_data;

    return dat;
}

void start_non_user_task_thread();

#endif // NON_USER_TASK_THREAD_HPP_INCLUDED
