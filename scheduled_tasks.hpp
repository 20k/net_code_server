#ifndef SCHEDULED_TASKS_HPP_INCLUDED
#define SCHEDULED_TASKS_HPP_INCLUDED

#include <map>
#include <string>
#include <SFML/System.hpp>
#include <vector>
#include <mutex>

struct scheduled_tasks
{
    struct task_data
    {
        sf::Clock clk;
        float timeout_s = 0;

        bool finished()
        {
            return clk.getElapsedTime().asMicroseconds() /  1000. / 1000. >= timeout_s;
        }
    };

    std::map<std::string, std::map<std::string, std::map<int, task_data>>> data_map;
    std::map<std::string, std::map<std::string, int>> counter_map;

    int task_register(const std::string& id, const std::string& task, float time_s)
    {
        std::lock_guard guard(mut);

        int counter = counter_map[id][task]++;

        data_map[id][task][counter].clk.restart();
        data_map[id][task][counter].timeout_s = time_s;

        return counter;
    }

    bool task_finished(const std::string& id, const std::string& task, int cnt)
    {
        std::lock_guard guard(mut);

        return data_map[id][task][cnt].finished();
    }

    std::vector<int> get_all_tasks(const std::string& id, const std::string& task)
    {
        std::lock_guard guard(mut);

        std::map<int, task_data> tasks = data_map[id][task];

        std::vector<int> vec;

        for(auto& i : tasks)
            vec.push_back(i.first);

        return vec;
    }

    void task_complete(const std::string& id, const std::string& task, int cnt)
    {
        std::lock_guard guard(mut);

        if(data_map[id][task].find(cnt) == data_map[id][task].end())
            return;

        data_map[id][task].erase(data_map[id][task].find(cnt));
    }

    //std::vector<std::string> fetch_all_tasks(const std::string& id)

    std::mutex mut;
};

scheduled_tasks& get_global_scheduled_tasks()
{
    static scheduled_tasks tasks;

    return tasks;
}

#endif // SCHEDULED_TASKS_HPP_INCLUDED
