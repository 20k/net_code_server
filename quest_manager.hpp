#ifndef QUEST_MANAGER_HPP_INCLUDED
#define QUEST_MANAGER_HPP_INCLUDED

#include "db_interfaceable.hpp"

namespace quest_type
{
    enum class type : int32_t
    {
        SEND_CASH_TO,
        STEAL_CASH_FROM,
        SEND_ITEM_TO,
        STEAL_ITEM_FROM,

        HACK_USER, ///front
        BREACH_USER, ///breach node
        ///hack item and gc logs?

        ///then expose breach, item and gc as quest options

        MOVE_TO_SYSTEM,

        CLAIM_USER_FOR, ///claim a user for someone
        REVOKE_USER_FROM, ///revoke a users access from another user

        RUN_SCRIPT,

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

        "Run Script",
    };
}

struct quest_type_base
{
    //void process(nlohmann::json& json);

    //void update_json(nlohmann::json& json) = 0;
};

struct quest_targeted_user : quest_type_base
{
    std::string target;

    void update_json(nlohmann::json& json);
};

struct quest_breach_data : quest_targeted_user
{

};

struct quest_hack_data : quest_targeted_user
{

};

struct quest_script_data : quest_type_base
{
    std::string target;

    void update_json(nlohmann::json& json);
};

struct quest_cash_send_data
{
    std::string target;
    double at_least = 0;

    void update_json(nlohmann::json& json);
};

struct quest : db_interfaceable<quest, MACRO_GET_STR("id")>
{
    ///who's this quest being done for?
    DB_VAL(std::string, user_for);

    DB_VAL(std::string, name);
    DB_VAL(std::string, description);

    using data_type = std::pair<quest_type::type, nlohmann::json>;
    using map_type = std::vector<data_type>;

    ///maps quest type to a user to arbitrary json
    ///so the way this is expected to operate is that we run a script, and that script keeps track of what we're doing
    ///also note to self: generally try to cut down on the number of threads in the application
    DB_VAL(map_type, quest_data);

    bool handle_serialise(json& j, bool ser) override
    {
        quest_data.serialise(j, ser);
        user_for.serialise(j, ser);

        name.serialise(j, ser);
        description.serialise(j, ser);

        return false;
    }

    bool is_index_completed(int idx);

    bool complete();

    nlohmann::json get_quest_part_data(quest_type::type t);
    void set_quest_part_data(quest_type::type t, const nlohmann::json& j);

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

    void add_run_script(const std::string& script_name);

    std::string get_as_string();
    nlohmann::json get_as_data();

    void send_new_quest_alert_to(int lock_id, const std::string& to);
};

struct quest_manager
{
    std::vector<quest> fetch_quests_of(mongo_lock_proxy& ctx, const std::string& user);

    quest get_new_quest_for(const std::string& username, const std::string& name, const std::string& description);

    void process(int lock_id, const std::string& caller, quest_cash_send_data& t);
    void process(int lock_id, const std::string& caller, quest_breach_data& t);
    void process(int lock_id, const std::string& caller, quest_hack_data& t);
    void process(int lock_id, const std::string& caller, quest_script_data& t);
};

inline
quest_manager& get_global_quest_manager()
{
    static quest_manager quest_manage;

    return quest_manage;
}

#endif // QUEST_MANAGER_HPP_INCLUDED
