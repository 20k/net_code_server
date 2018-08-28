#ifndef QUEST_MANAGER_HPP_INCLUDED
#define QUEST_MANAGER_HPP_INCLUDED

#include "db_interfaceable.hpp"

struct quest_manager : db_interfaceable<quest_manager, true, MACRO_GET_STR("id")>
{
    enum class type
    {
        ADA_TUTORIAL,
    };

    using map_type = std::vector<std::pair<type, nlohmann::json>>;

    ///maps quest type to a user to arbitrary json
    ///so the way this is expected to operate is that we run a script, and that script keeps track of what we're doing
    ///also note to self: generally try to cut down on the number of threads in the application
    DB_VAL(map_type, quest_data);

    bool handle_serialise(json& j, bool ser)
    {
        quest_data.serialise(j, ser);

        return false;
    }

    nlohmann::json get_quest_data(type t, const std::string& username);
    void set_quest_data(type t, const std::string& username, const nlohmann::json& j);
};

#endif // QUEST_MANAGER_HPP_INCLUDED
