#include "quest_manager.hpp"

nlohmann::json quest::get_quest_part_data(quest::type t)
{
    for(auto& i : *quest_data)
    {
        if(i.first == t)
            return i.second;
    }

    return nlohmann::json();
}

void quest::set_quest_part_data(type t, const nlohmann::json& j)
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

void quest::add_hack_user(const std::string& target)
{
    data_type dat;
    dat.first = type::HACK_USER;
    dat.second["user"] = target;

    quest_data->push_back(dat);
}

void quest::add_breach_user(const std::string& target)
{
    data_type dat;
    dat.first = type::BREACH_USER;
    dat.second["user"] = target;

    quest_data->push_back(dat);
}

std::vector<quest> quest_manager::fetch_quests_of(mongo_lock_proxy& ctx, const std::string& user)
{
    nlohmann::json req;
    req["user_for"] = user;

    auto found = ctx->find_json_new(req, nlohmann::json());

    auto quests = quest::convert_all_from(found);

    return quests;
}

quest quest_manager::get_new_quest_for(const std::string& username)
{
    quest nquest;
    *nquest.user_for = username;

    return nquest;
}
