#ifndef EVENT_MANAGER_HPP_INCLUDED
#define EVENT_MANAGER_HPP_INCLUDED

#include "db_interfaceable.hpp"
#include "mongo.hpp"

namespace event
{
    struct event_impl : db_interfaceable<event_impl, MACRO_GET_STR("id")>
    {
        DB_VAL(std::string, user_name);
        DB_VAL(std::string, unique_event_tag);
        DB_VAL(bool, complete);

        bool handle_serialise(nlohmann::json& j, bool ser) override
        {
            user_name.serialise(j, ser);
            unique_event_tag.serialise(j, ser);
            complete.serialise(j, ser);

            return false;
        }
    };

    static inline std::mutex in_memory_lock;
    static inline std::map<std::string, std::map<std::string, bool>> in_memory_map;

    template<typename T>
    void exec_once_ever(const std::string& user_name, const std::string& unique_event_tag, const T& func)
    {
        {
            mongo_nolock_proxy ctx = get_global_mongo_event_manager_context(-2);

            nlohmann::json req;
            req["user_name"] = user_name;
            req["unique_event_tag"] = unique_event_tag;
            req["complete"] = true;

            auto found = ctx->find_json_new(req, nlohmann::json());

            if(found.size() > 0)
                return;
        }

        ///the reason why this is here is so that we check whether or not the function has executed
        ///any times before. But the persisting of this information to disk is only done *after* func is executed
        ///so that if the server crashes during the execution of func, the player isn't unfairly penalised or misses information
        {
            std::lock_guard guard(in_memory_lock);

            if(in_memory_map[user_name][unique_event_tag])
                return;

            in_memory_map[user_name][unique_event_tag] = true;
        }

        func();

        {
            mongo_nolock_proxy ctx = get_global_mongo_event_manager_context(-2);

            event_impl evt;
            evt.user_name = user_name;
            evt.unique_event_tag = unique_event_tag;
            evt.complete = true;
            evt.set_key_data(std::to_string(db_storage_backend::get_unique_id()));

            evt.overwrite_in_db(ctx);
        }
    }
}

#endif // EVENT_MANAGER_HPP_INCLUDED
