#ifndef SCHEDULED_TASKS_HPP_INCLUDED
#define SCHEDULED_TASKS_HPP_INCLUDED

#include <map>
#include <string>
#include <SFML/System.hpp>
#include <vector>
#include <mutex>
#include <functional>
#include <thread>
#include <json/json.hpp>
#include "db_interfaceable.hpp"

struct scheduled_tasks;

void task_thread(scheduled_tasks& tasks);

void on_finish_relink(int cnt, const std::vector<std::string>& data);
void on_disconnect_link(int cnt, const std::vector<std::string>& data);
void on_heal_network_link(int cnt, const std::vector<std::string>& data);

extern double get_wall_time_s();

namespace task_type
{
    enum task_type
    {
        ON_RELINK,
        ON_DISCONNECT,
        ON_HEAL_NETWORK,
    };
}

struct task_data_db : db_interfaceable<task_data_db, true, MACRO_GET_STR("id")>
{
    DB_VAL(float, start_time_s);
    DB_VAL(float, end_time_s);
    DB_VAL(bool, called_callback);
    DB_VAL(task_type::task_type, type);
    DB_VAL(std::vector<std::string>, udata);
    DB_VAL(int, count_offset);

    bool handle_serialise(json& j, bool ser) override
    {
        start_time_s.serialise(j, ser);
        end_time_s.serialise(j, ser);
        called_callback.serialise(j, ser);
        type.serialise(j, ser);
        udata.serialise(j, ser);
        count_offset.serialise(j, ser);

        return false;
    }

    bool finished()
    {
        return get_wall_time_s() >= end_time_s;
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

            all = task_data_db::fetch_all_from_db(ctx);
        }

        {
            std::lock_guard guard(mut);

            for(auto& i : all)
            {
                task_data[i.count_offset] = i;

                counter = std::max(counter, (int)i.count_offset);
            }

            counter++;
        }

        std::thread(task_thread, std::ref(*this)).detach();
    }

    void handle_callback(task_data_db& d)
    {
        if(d.type == task_type::ON_RELINK)
        {
            on_finish_relink(d.count_offset, (std::vector<std::string>)d.udata);
        }

        if(d.type == task_type::ON_DISCONNECT)
        {
            on_disconnect_link(d.count_offset, (std::vector<std::string>)d.udata);
        }

        if(d.type == task_type::ON_HEAL_NETWORK)
        {
            on_heal_network_link(d.count_offset, (std::vector<std::string>)d.udata);
        }

        {
            mongo_lock_proxy ctx = get_global_mongo_scheduled_task_context(-2);

            d.remove_from_db(ctx);
        }
    }

    int task_register(const task_type::task_type& task, float time_s, const std::vector<std::string>& data, int thread_id)
    {
        task_data_db tdd;
        tdd.start_time_s = get_wall_time_s();
        tdd.end_time_s = tdd.start_time_s + time_s;
        tdd.type = task;
        tdd.udata = data;

        int cnt = 0;

        {
            std::lock_guard guard(mut);

            cnt = counter++;
            tdd.count_offset = cnt;
            tdd.set_key_data(std::to_string(cnt));

            task_data[cnt] = tdd;
        }

        {
            mongo_lock_proxy ctx = get_global_mongo_scheduled_task_context(thread_id);

            tdd.overwrite_in_db(ctx);
        }

        return cnt;
    }

    std::vector<task_data_db> get_all_tasks_of_type(task_type::task_type task, int thread_id)
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
        std::lock_guard guard(mut);
        std::vector<int> to_erase;

        for(auto& i : task_data)
        {
            task_data_db& d = i.second;

            if(d.finished() && !d.called_callback)
            {
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
