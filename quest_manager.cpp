#include "quest_manager.hpp"
#include <libncclient/nc_util.hpp>

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

std::string quest::get_as_string()
{
    std::string ret;

    int dim = quest_data->size();

    ret = *name + ":\n";
    ret += *description + "\n";

    for(int i=0; i < dim; i++)
    {
        std::string title = quest::type_strings[i] + ": ";

        data_type& type = (*quest_data)[i];

        bool complete = type.second["complete"];

        if(type.first == quest::type::HACK_USER)
        {
            std::string usr = type.second["user"];

            ret += colour_string_if(title, complete) + usr + "\n";
        }

        if(type.first == quest::type::BREACH_USER)
        {
            std::string usr = type.second["user"];

            ret += colour_string_if(title, complete) + usr + "\n";
        }
    }

    if(dim > 0)
        ret.pop_back();

    return ret;
}

nlohmann::json quest::get_as_data()
{
    std::vector<nlohmann::json> js;

    for(int i=0; i < (int)quest_data->size(); i++)
    {
        js.push_back((*quest_data)[i]);
    }

    nlohmann::json ret;
    ret["user"] = *user_for;
    ret["quests"] = js;
    ret["name"] = *name;
    ret["description"] = *description;

    return ret;
}

std::vector<quest> quest_manager::fetch_quests_of(mongo_lock_proxy& ctx, const std::string& user)
{
    nlohmann::json req;
    req["user_for"] = user;

    auto found = ctx->find_json_new(req, nlohmann::json());

    auto quests = quest::convert_all_from(found);

    return quests;
}

quest quest_manager::get_new_quest_for(const std::string& username, const std::string& name, const std::string& description)
{
    quest nquest;
    *nquest.user_for = username;
    *nquest.name = name;
    *nquest.description = description;

    return nquest;
}
