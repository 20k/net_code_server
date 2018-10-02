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

    static inline std::vector<std::string> type_strings
    {
        "Deposit Cash",
        "Steal Cash",
        "Send Item",
        "Steal Item",

        "Poke User",
        "Breach User",

        "Move to System",

        "Claim User For",
        "Revoke User From",
    };

    ///who's this quest being done for?
    DB_VAL(std::string, user_for);

    DB_VAL(std::string, name);
    DB_VAL(std::string, description);

    using data_type = std::pair<type, nlohmann::json>;
    using map_type = std::vector<data_type>;

    ///maps quest type to a user to arbitrary json
    ///so the way this is expected to operate is that we run a script, and that script keeps track of what we're doing
    ///also note to self: generally try to cut down on the number of threads in the application
    DB_VAL(map_type, quest_data);

    bool handle_serialise(json& j, bool ser)
    {
        quest_data.serialise(j, ser);
        user_for.serialise(j, ser);

        name.serialise(j, ser);
        description.serialise(j, ser);

        return false;
    }

    nlohmann::json get_quest_part_data(type t);
    void set_quest_part_data(type t, const nlohmann::json& j);

    void add_send_cash(const std::string& to, double amount);
    void add_steal_cash(const std::string& from, double amount);

    /*
    ///steal item type?
    void add_send_item(const std::string& to, double amount);
    void add_steal_item(const std::string& from, double amount);*/

    void add_hack_user(const std::string& target);
    void add_breach_user(const std::string& target);

    void add_move_system(const std::string& sys);

    void add_claim_user_for(const std::string& claim_user, const std::string& claim_for);
    void add_revoke_user_from(const std::string& revoke_user, const std::string& revoke_from);

    std::string get_as_string();
    nlohmann::json get_as_data();
};

struct quest_manager
{
    std::vector<quest> fetch_quests_of(mongo_lock_proxy& ctx, const std::string& user);

    quest get_new_quest_for(const std::string& username, const std::string& name, const std::string& description);
};

#endif // QUEST_MANAGER_HPP_INCLUDED
