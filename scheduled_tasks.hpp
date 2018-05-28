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

void on_finish_relink(const std::string& id, int cnt, const std::vector<std::string>& data);

extern double get_wall_time_s();

namespace task_type
{
    enum task_type
    {
        ON_RELINK,
    };
}

struct task_data_db : db_interfaceable<task_data_db, true, MACRO_GET_STR("data_type")>
{
    /*db_val<float, task_data_db> end_time_s = 0;
    db_val<bool, task_data_db> called_callback = false;
    db_val<task_type::task_type, task_data_db> type;
    db_val<std::vector<std::string>, task_data_db> data;*/

    DB_VAL(float, end_time_s);
    DB_VAL(bool, called_callback);
    DB_VAL(task_type::task_type, type);
    DB_VAL(std::vector<std::string>, udata);

    bool handle_serialise(json& j, bool ser) override
    {
        end_time_s.serialise(j, ser);
        called_callback.serialise(j, ser);
        type.serialise(j, ser);
        udata.serialise(j, ser);

        return false;
    }
};

struct scheduled_tasks
{
    struct task_data
    {
        float end_time_s = 0;

        bool called_callback = false;
        task_type::task_type type;
        std::vector<std::string> data;

        bool finished()
        {
            return get_wall_time_s() >= end_time_s;
        }

        nlohmann::json get()
        {
            nlohmann::json j;
            j["end_time_s"] = end_time_s;
            j["called_callback"] = called_callback;
            j["type"] = type;
            j["data"] = data;

            return j;
        }

        void set(nlohmann::json& j)
        {
            end_time_s = j["end_time_s"];
            called_callback = j["called_callback"];
            type = j["type"];
            data = j["data"].get<std::vector<std::string>>();
        }
    };

    std::map<std::string, std::map<task_type::task_type, std::map<int, task_data>>> data_map;
    std::map<std::string, std::map<task_type::task_type, int>> counter_map;

    int task_register(const std::string& id, const task_type::task_type& task, float time_s, const std::vector<std::string>& data)
    {
        std::lock_guard guard(mut);

        int counter = counter_map[id][task]++;

        data_map[id][task][counter].end_time_s = get_wall_time_s() + time_s;
        data_map[id][task][counter].type = task;
        data_map[id][task][counter].data = data;

        return counter;
    }

    bool task_finished(const std::string& id, const task_type::task_type& task, int cnt)
    {
        std::lock_guard guard(mut);

        return data_map[id][task][cnt].finished();
    }

    task_data get_task(const std::string& id, const task_type::task_type& task, int cnt)
    {
        std::lock_guard guard(mut);

        return data_map[id][task][cnt];
    }

    std::vector<int> get_all_tasks(const std::string& id, const task_type::task_type& task)
    {
        std::lock_guard guard(mut);

        std::map<int, task_data> tasks = data_map[id][task];

        std::vector<int> vec;

        for(auto& i : tasks)
            vec.push_back(i.first);

        return vec;
    }

    void task_complete(const std::string& id, const task_type::task_type& task, int cnt)
    {
        std::lock_guard guard(mut);

        if(data_map[id][task].find(cnt) == data_map[id][task].end())
            return;

        data_map[id][task].erase(data_map[id][task].find(cnt));
    }

    void handle_callback(task_data& d, const std::string& id, int cnt)
    {
        if(d.type == task_type::ON_RELINK)
        {
            on_finish_relink(id, cnt, d.data);
        }
    }

    void check_all_tasks(double dt_s)
    {
        std::lock_guard guard(mut);

        for(auto& i : data_map)
        {
            for(auto& j : i.second)
            {
                std::vector<int> to_erase;

                for(auto& k : j.second)
                {
                    task_data& d = k.second;

                    if(d.finished() && !d.called_callback)
                    {
                        //d.callback(i.first, j.first, k.first);

                        handle_callback(d, i.first, k.first);

                        d.called_callback = true;

                        to_erase.push_back(k.first);
                    }
                }

                for(auto& k : to_erase)
                {
                    j.second.erase(j.second.find(k));
                }
            }
        }
    }

    scheduled_tasks()
    {
        std::thread(task_thread, std::ref(*this)).detach();
    }

    //std::vector<std::string> fetch_all_tasks(const std::string& id)

    std::recursive_mutex mut;
};

inline
scheduled_tasks& get_global_scheduled_tasks()
{
    static scheduled_tasks tasks;

    return tasks;
}

#endif // SCHEDULED_TASKS_HPP_INCLUDED
