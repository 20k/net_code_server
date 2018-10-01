#ifndef QUEST_MANAGER_HPP_INCLUDED
#define QUEST_MANAGER_HPP_INCLUDED

#include "db_interfaceable.hpp"

struct quest : db_interfaceable<quest, MACRO_GET_STR("id")>
{
    /*enum class type
    {
        ADA_TUTORIAL,
    };*/

    enum class type : int32_t
    {
        SEND_CASH_TO,
        STEAL_CASH_FROM,
        SEND_ITEM_TO,
        STEAL_ITEM_FROM,

        HACK_USER, ///front
        BREACH_USER, ///breach node

        MOVE_TO_SYSTEM,

        CLAIM_USER_FOR, ///claim a user for someone
        REVOKE_USER_FROM, ///revoke a users access from another user

        ///create a script that does something?
    };

    ///who's this quest being done for?
    DB_VAL(std::string, user_for);

    using map_type = std::vector<std::pair<type, nlohmann::json>>;

    ///maps quest type to a user to arbitrary json
    ///so the way this is expected to operate is that we run a script, and that script keeps track of what we're doing
    ///also note to self: generally try to cut down on the number of threads in the application
    DB_VAL(map_type, quest_data);

    bool handle_serialise(json& j, bool ser)
    {
        quest_data.serialise(j, ser);
        user_for.serialise(j, ser);

        return false;
    }

    nlohmann::json get_quest_part_data(type t);
    void set_quest_part_data(type t, const nlohmann::json& j);
};

struct quest_manager
{
    std::vector<quest> fetch_quests_of(mongo_lock_proxy& ctx, const std::string& user);
};

#endif // QUEST_MANAGER_HPP_INCLUDED
