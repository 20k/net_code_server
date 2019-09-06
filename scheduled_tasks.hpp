#ifndef SCHEDULED_TASKS_HPP_INCLUDED
#define SCHEDULED_TASKS_HPP_INCLUDED

#include <map>
#include <string>
#include <SFML/System.hpp>
#include <vector>
#include <mutex>
#include <thread>
#include "safe_thread.hpp"
#include "serialisables.hpp"
#include <networking/serialisable_fwd.hpp>
#include "mongo.hpp"

struct scheduled_tasks;

void task_thread(scheduled_tasks& tasks);

void on_finish_relink(int cnt, std::vector<std::string> data);
void on_disconnect_link(int cnt, std::vector<std::string> data);
void on_heal_network_link(int cnt, std::vector<std::string> data);
void on_force_disconnect_link(int cnt, std::vector<std::string> data);

extern double get_wall_time_s();

namespace task_type
{
    enum task_type
    {
        ON_RELINK,
        ON_DISCONNECT,
        ON_HEAL_NETWORK,
        ON_FORCE_DISCONNECT,
    };
}

struct task_data_db : serialisable, free_function
{
    std::string id;
    double start_time_s;
    double end_time_s;
    bool called_callback;
    task_type::task_type type;
    std::vector<std::string> udata;
    int count_offset;

    bool finished()
    {
        return get_wall_time_s() >= (double)end_time_s;
    }
};

struct scheduled_tasks
{
    std::recursive_mutex mut;
    std::map<int, task_data_db> task_data;

    int counter = 0;

    scheduled_tasks()
    {
        std::vector<task_data_db> all;

        {
            mongo_lock_proxy ctx = get_global_mongo_scheduled_task_context(-2);

            all = db_disk_load_all(ctx, task_data_db());
        }

        {
            safe_lock_guard guard(mut);

            for(auto& i : all)
            {
                task_data[i.count_offset] = i;

                counter = std::max(counter, (int)i.count_offset);
            }

            counter++;
        }

        sthread(task_thread, std::ref(*this)).detach();
    }

    void handle_callback(task_data_db& d)
    {
        if(d.type == task_type::ON_RELINK)
        {
             sthread(on_finish_relink, d.count_offset, (std::vector<std::string>)d.udata).detach();
        }

        if(d.type == task_type::ON_DISCONNECT)
        {
             sthread(on_disconnect_link, d.count_offset, (std::vector<std::string>)d.udata).detach();
        }

        if(d.type == task_type::ON_HEAL_NETWORK)
        {
             sthread(on_heal_network_link, d.count_offset, (std::vector<std::string>)d.udata).detach();
        }

        if(d.type == task_type::ON_FORCE_DISCONNECT)
        {
             sthread(on_force_disconnect_link, d.count_offset, (std::vector<std::string>)d.udata).detach();
        }

        {
            mongo_lock_proxy ctx = get_global_mongo_scheduled_task_context(-2);

            db_disk_remove(ctx, d);
        }
    }

    int task_register(const task_type::task_type& task, double time_s, const std::vector<std::string>& data, int thread_id)
    {
        task_data_db tdd;
        tdd.start_time_s = get_wall_time_s();
        tdd.end_time_s = tdd.start_time_s + time_s;
        tdd.type = task;
        tdd.udata = data;

        //std::cout << "register task " << tdd.type.value() << " " << tdd.start_time_s.value() << " " << tdd.end_time_s.value() << std::endl;

        int cnt = 0;

        {
            safe_lock_guard guard(mut);

            cnt = counter++;
            tdd.count_offset = cnt;
            tdd.id = std::to_string(cnt);

            task_data[cnt] = tdd;
        }

        {
            mongo_nolock_proxy ctx = get_global_mongo_scheduled_task_context(thread_id);

            db_disk_overwrite(ctx, tdd);
        }

        return cnt;
    }

    std::vector<task_data_db> get_all_tasks_of_type(task_type::task_type task)
    {
        std::vector<task_data_db> all;

        for(auto& i : task_data)
        {
            if(i.second.type == task)
                all.push_back(i.second);
        }

        return all;
    }

    void check_all_tasks(double dt_s)
    {
        safe_lock_guard guard(mut);
        std::vector<int> to_erase;

        for(auto& i : task_data)
        {
            task_data_db& d = i.second;

            if(d.finished() && !d.called_callback)
            {
                //std::cout << "exec task " << d.type.value() << " " << d.start_time_s.value() << " " << d.end_time_s.value() << std::endl;

                handle_callback(d);

                d.called_callback = true;

                to_erase.push_back(i.first);
            }
        }

        for(auto& k : to_erase)
        {
            task_data.erase(task_data.find(k));
        }
    }
};

inline
scheduled_tasks& get_global_scheduled_tasks()
{
    static scheduled_tasks tasks;

    return tasks;
}

#endif // SCHEDULED_TASKS_HPP_INCLUDED
