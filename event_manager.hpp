#ifndef EVENT_MANAGER_HPP_INCLUDED
#define EVENT_MANAGER_HPP_INCLUDED

#include "mongo.hpp"
#include "safe_thread.hpp"
#include <networking/serialisable_fwd.hpp>
#include "serialisables.hpp"

struct event_impl : serialisable, free_function
{
    std::string id;
    std::string user_name;
    std::string unique_event_tag;
    bool complete = false;
};

namespace event
{
    struct db_saver
    {
        bool owns = false;
        event_impl evt;

        static inline std::mutex transitory_lock;
        static inline std::map<std::string, std::map<std::string, bool>> transitory_map;

        static bool in_progress(event_impl& _evt)
        {
            auto i1 = transitory_map.find(_evt.user_name);

            if(i1 == transitory_map.end())
                return false;

            auto i2 = i1->second.find(_evt.unique_event_tag);

            if(i2 == i1->second.end())
                return false;

            return true;
        }

        db_saver(){}

        db_saver(event_impl& _evt)
        {
            evt = _evt;
            owns = true;

            {
                safe_lock_guard slg(transitory_lock);

                transitory_map[evt.user_name][evt.unique_event_tag] = true;
            }
        }

        db_saver(db_saver&& other)
        {
            owns = other.owns;
            evt = other.evt;

            other.owns = false;
        }

        db_saver(const db_saver&) = delete;
        db_saver& operator=(const db_saver&) = delete;

        ~db_saver()
        {
            if(!owns)
                return;

            {
                mongo_nolock_proxy ctx = get_global_mongo_event_manager_context(-2);
                ctx.change_collection(evt.user_name);

                db_disk_overwrite(ctx, evt);
            }

            auto i1 = transitory_map.find(evt.user_name);

            if(i1 == transitory_map.end())
                return;

            auto i2 = i1->second.find(evt.unique_event_tag);

            if(i2 == i1->second.end())
                return;

            i1->second.erase(i2);
        }
    };

    static inline std::mutex in_memory_lock;
    static inline std::map<std::string, std::map<std::string, bool>> in_memory_map;

    inline
    bool was_executed(const std::string& user_name, const std::string& unique_event_tag)
    {
        {
            mongo_nolock_proxy ctx = get_global_mongo_event_manager_context(-2);
            ctx.change_collection(user_name);

            nlohmann::json req;
            req["user_name"] = user_name;
            req["unique_event_tag"] = unique_event_tag;
            req["complete"] = true;

            auto found = ctx->find_json_new(req, nlohmann::json());

            if(found.size() > 0)
                return true;
        }

        {
            std::lock_guard guard(in_memory_lock);

            if(in_memory_map[user_name][unique_event_tag])
                return true;
        }

        return false;
    }

    inline
    void reset(const std::string& user_name, const std::string& unique_event_tag)
    {
        {
            mongo_nolock_proxy ctx = get_global_mongo_event_manager_context(-2);
            ctx.change_collection(user_name);

            nlohmann::json req;
            req["user_name"] = user_name;
            req["unique_event_tag"] = unique_event_tag;
            req["complete"] = true;

            ctx->remove_json_many_new(req);
        }

        {
            std::lock_guard guard(in_memory_lock);

            auto i1 = in_memory_map.find(user_name);

            if(i1 == in_memory_map.end())
                return;

            auto i2 = i1->second.find(unique_event_tag);

            if(i2 == i1->second.end())
                return;

            i1->second.erase(i2);
        }
    }

    template<typename T>
    inline
    bool exec_once_ever(const std::string& user_name, const std::string& unique_event_tag, const T& func)
    {
        if(was_executed(user_name, unique_event_tag))
            return false;

        ///the reason why this is here is so that we check whether or not the function has executed
        ///any times before. But the persisting of this information to disk is only done *after* func is executed
        ///so that if the server crashes during the execution of func, the player isn't unfairly penalised or misses information
        {
            std::lock_guard guard(in_memory_lock);

            in_memory_map[user_name][unique_event_tag] = true;
        }

        event_impl evt;
        evt.user_name = user_name;
        evt.unique_event_tag = unique_event_tag;
        evt.complete = true;
        evt.id = std::to_string(db_storage_backend::get_unique_id());

        if(db_saver::in_progress(evt))
            return false;

        db_saver save(evt);

        func(std::move(save));

        /*{
            mongo_nolock_proxy ctx = get_global_mongo_event_manager_context(-2);
            ctx.change_collection(user_name);

            event_impl evt;
            evt.user_name = user_name;
            evt.unique_event_tag = unique_event_tag;
            evt.complete = true;
            evt.set_key_data(std::to_string(db_storage_backend::get_unique_id()));

            evt.overwrite_in_db(ctx);
        }*/

        return true;
    }
}

#endif // EVENT_MANAGER_HPP_INCLUDED
