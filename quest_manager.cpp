#include "quest_manager.hpp"
#include <libncclient/nc_util.hpp>
#include "privileged_core_scripts.hpp"
#include "command_handler.hpp"
#include "serialisables.hpp"
#include <networking/serialisable.hpp>

void quest_targeted_user::update_json(nlohmann::json& json)
{
    if(json.count("user") == 0)
        return;

    if(json.at("user") == target)
    {
        json["completed"] = true;
    }
}

void quest_script_data::update_json(nlohmann::json& json)
{
    if(json.count("script") == 0)
        return;

    if(json.at("script") == target)
    {
        json["completed"] = true;
    }
}

void quest_cash_send_data::update_json(nlohmann::json& json)
{
    if(json.count("user") == 0)
        return;

    if(json.at("user") != target)
        return;

    if(json.count("current_amount") == 0 || json.count("target_amount") == 0)
        return;

    double cur = (double)json.at("current_amount") + at_least;
    double max = json.at("target_amount");

    if(cur >= max)
    {
        json["completed"] = true;
    }

    json["current_amount"] = cur;
}

bool quest::is_index_completed(int idx)
{
    if(idx < 0 || idx >= (int)quest_data.size())
        return true;

    nlohmann::json overall_data = quest_data[idx].second;

    if(overall_data.count("completed") == 0)
        return false;

    nlohmann::json data = quest_data[idx].second["completed"];

    if(!data.is_boolean())
        return true;

    return (bool)data;
}

bool quest::complete()
{
    for(int i=0; i < (int)quest_data.size(); i++)
    {
        if(!is_index_completed(i))
            return false;
    }

    return true;
}

nlohmann::json quest::get_quest_part_data(quest_type::type t)
{
    for(auto& i : quest_data)
    {
        if(i.first == t)
            return i.second;
    }

    return nlohmann::json();
}

void quest::set_quest_part_data(quest_type::type t, const nlohmann::json& j)
{
    for(auto& i : quest_data)
    {
        if(i.first == t)
        {
            i.second = j;
            return;
        }
    }

    quest_data.push_back({t, j});
}

void quest::add_send_cash(const std::string& target, double amount)
{
    data_type dat;
    dat.first = quest_type::type::SEND_CASH_TO;
    dat.second["user"] = target;
    dat.second["target_amount"] = amount;
    dat.second["current_amount"] = 0.;

    quest_data.push_back(dat);
}

void quest::add_hack_user(const std::string& target)
{
    data_type dat;
    dat.first = quest_type::type::HACK_USER;
    dat.second["user"] = target;

    quest_data.push_back(dat);
}

void quest::add_breach_user(const std::string& target)
{
    data_type dat;
    dat.first = quest_type::type::BREACH_USER;
    dat.second["user"] = target;

    quest_data.push_back(dat);
}

void quest::add_run_script(const std::string& script_name)
{
    data_type dat;
    dat.first = quest_type::type::RUN_SCRIPT;
    dat.second["script"] = script_name;

    quest_data.push_back(dat);
}

std::string quest::get_as_string()
{
    std::string ret;

    int dim = quest_data.size();

    bool is_complete = complete();

    std::string name_col = colour_string(name);

    if(is_complete)
    {
        name_col += make_success_col(" (complete)");

        //name_col = make_success_col(name_col) + " (finished)";
    }
    else
    {
        name_col += make_error_col(" (incomplete)");

        //name_col = make_error_col(name_col) + " (incomplete)";
    }

    ret = name_col + "\n";

    ret += "Description:\n" + description + "\n";

    ret += "Tasks:\n";

    for(int i=0; i < dim; i++)
    {
        data_type& type = quest_data[i];

        std::string title = quest_type::type_strings[(int)type.first];

        bool complete = is_index_completed(i);

        if(type.first == quest_type::type::SEND_CASH_TO)
        {
            std::string usr = type.second["user"];

            double current_amount = type.second["current_amount"];
            double max_amount = type.second["target_amount"];

            ret += colour_string(title) + ": " +
            to_string_with_enforced_variable_dp(current_amount, 2) + "/" +
            to_string_with_enforced_variable_dp(max_amount, 2) + " to " + colour_string(usr);
        }

        if(type.first == quest_type::type::HACK_USER)
        {
            std::string usr = type.second["user"];

            ret += colour_string(title) + ": " + colour_string(usr);
        }

        if(type.first == quest_type::type::BREACH_USER)
        {
            std::string usr = type.second["user"];

            ret += colour_string(title) + ": " + colour_string(usr);
        }

        if(type.first == quest_type::type::RUN_SCRIPT)
        {
            std::string script = type.second["script"];

            ret += colour_string(title) + ": #" + script + "()";
        }

        if(complete)
        {
            ret += make_success_col(" (complete)");
        }
        else
        {
            ret += make_error_col(" (incomplete)");
        }

        ret += "\n";
    }

    ret = strip_trailing_newlines(ret);

    return ret;
}

nlohmann::json quest::get_as_data()
{
    std::vector<nlohmann::json> js;

    for(int i=0; i < (int)quest_data.size(); i++)
    {
        js.push_back(quest_data[i]);
    }

    nlohmann::json ret;
    ret["user"] = user_for;
    ret["quests"] = js;
    ret["name"] = name;
    ret["description"] = description;

    return ret;
}

void quest::set_on_finish(const std::string& on_finish)
{
    run_on_complete = on_finish;
}

std::vector<quest> quest_manager::fetch_quests_of(db::read_tx& ctx, const std::string& user)
{
    std::vector<quest> all_quests = db_disk_load_all(ctx, quest());
    std::vector<quest> ret;

    for(quest& q : all_quests)
    {
        if(q.user_for == user)
            ret.push_back(q);
    }

    return ret;
}

quest quest_manager::get_new_quest_for(const std::string& username, const std::string& name, const std::string& description)
{
    quest nquest;
    nquest.user_for = username;
    nquest.name = name;
    nquest.description = description;

    {
        db::read_write_tx tx;
        nquest.id = std::to_string(db::get_next_id(tx));
    }

    return nquest;
}


void quest::send_new_quest_alert_to(int lock_id, const std::string& to)
{
    std::string notif = "New Mission Received:\n" + get_as_string() + "\n";

    create_notification(to, notif);
}

enum class quest_state
{
    NONE,
    PARTIAL,
    COMPLETE
};

template<typename T>
quest_state quest_process(quest& q, T& t)
{
    quest_state ret = quest_state::NONE;

    for(int i=0; i < (int)q.quest_data.size(); i++)
    {
        quest::data_type& type = q.quest_data[i];

        if(q.is_index_completed(i))
            continue;

        if(type.first != T::class_type)
            continue;

        t.update_json(type.second);

        if(q.is_index_completed(i))
        {
            ret = quest_state::COMPLETE;
        }
        else
        {
            ret = quest_state::PARTIAL;
        }
    }

    return ret;
}

template<typename T>
void process_qm(quest_manager& qm, int lock_id, const std::string& caller, T& t)
{
    std::string str;

    {
        mongo_lock_proxy ctx = get_global_mongo_quest_manager_context(lock_id);

        auto quests_for = qm.fetch_quests_of(ctx, caller);

        //for(auto& i : quests_for)
        for(int idx = 0; idx < (int)quests_for.size(); idx++)
        {
            auto result = quest_process(quests_for[idx], t);

            if(result == quest_state::PARTIAL || result == quest_state::COMPLETE)
            {
                db_disk_overwrite(ctx, quests_for[idx]);

                if(result == quest_state::COMPLETE)
                    str += std::to_string(idx) + ". " + quests_for[idx].get_as_string() + "\n\n";
            }
        }

        for(auto& i : quests_for)
        {
            if(!i.complete())
                continue;

            std::string run = i.run_on_complete;

            if(run != "")
            {
                throwaway_user_thread(caller, run, std::nullopt, true);
            }

            db_disk_remove(ctx, i);
        }
    }

    str = strip_trailing_newlines(str);

    if(str != "")
    {
        create_notification(caller, "Completed:\n" + str + "\n");
    }
}

void quest_manager::process(int lock_id, const std::string& caller, quest_cash_send_data& t)
{
    return process_qm(*this, lock_id, caller, t);
}

void quest_manager::process(int lock_id, const std::string& caller, quest_breach_data& t)
{
    return process_qm(*this, lock_id, caller, t);
}

void quest_manager::process(int lock_id, const std::string& caller, quest_hack_data& t)
{
    return process_qm(*this, lock_id, caller, t);
}

void quest_manager::process(int lock_id, const std::string& caller, quest_script_data& t)
{
    return process_qm(*this, lock_id, caller, t);
}
