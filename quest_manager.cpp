#include "quest_manager.hpp"

nlohmann::json quest_manager::get_quest_data(quest_manager::type t, const std::string& username)
{
    for(auto& i : *quest_data)
    {
        if(i.first == t)
            return i.second;
    }

    return nlohmann::json();
}

void quest_manager::set_quest_data(type t, const std::string& username, const nlohmann::json& j)
{
    for(auto& i : *quest_data)
    {
        if(i.first == t)
        {
            i.second = j;
            return;
        }
    }

    quest_data->push_back({t, j});
}
