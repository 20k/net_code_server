#ifndef RATE_LIMITING_HPP_INCLUDED
#define RATE_LIMITING_HPP_INCLUDED

#include <mutex>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include "safe_thread.hpp"

namespace rate
{
    enum rates
    {
        CHAT,
        CASH,
        UPG_CHEAT,
        AUTOCOMPLETES,
        DELETE_USER, ///does not persist between server restarts
        POLL,
        ASYNC_PRINT,
        CREATE_CHANNEL,
    };
}

using rate_limit_t = rate::rates;

struct rate_limit
{
    ///maps username to a type of rate limit
    std::map<std::string, std::map<rate_limit_t, double>> time_budget_remaining;
    std::map<std::string, std::map<std::string, double>> time_budget_remaining_manual;
    std::map<rate_limit_t, double> max_reserve = {{rate::CHAT, 10}, {rate::CASH, 30}, {rate::UPG_CHEAT, 10}, {rate::AUTOCOMPLETES, 30}, {rate::DELETE_USER, 61*60}, {rate::POLL, 5}, {rate::ASYNC_PRINT, 60}, {rate::CREATE_CHANNEL, 10}};
    std::map<rate_limit_t, double> budget_deplete = {{rate::CHAT, 1}, {rate::CASH, 1}, {rate::UPG_CHEAT, 3}, {rate::AUTOCOMPLETES, 1}, {rate::DELETE_USER, 60*60}, {rate::POLL, 0.25f}, {rate::ASYNC_PRINT, 1/120.f}, {rate::CREATE_CHANNEL, 1}};

    std::mutex lock;

    bool try_call(const std::string& usr_name, rate_limit_t type)
    {
        safe_lock_guard gd(lock);

        if(time_budget_remaining.find(usr_name) == time_budget_remaining.end())
        {
            time_budget_remaining[usr_name] = max_reserve;
        }

        double& num_remaining = time_budget_remaining[usr_name][type];

        if(num_remaining - budget_deplete[type] <= 0)
            return false;

        num_remaining -= budget_deplete[type];

        return true;
    }

    bool try_call_manual(const std::string& usr_name, const std::string& name, double mreserve, double mdeplete, double mmax)
    {
        safe_lock_guard gd(lock);

        if(time_budget_remaining_manual.find(usr_name) == time_budget_remaining_manual.end())
        {
            time_budget_remaining_manual[usr_name][name] = mreserve;
        }

        if(time_budget_remaining_manual[usr_name][name] > mmax)
        {
            time_budget_remaining_manual[usr_name][name] = mmax;
        }

        double& num_remaining = time_budget_remaining_manual[usr_name][name];

        if(num_remaining - mdeplete <= 0)
            return false;

        num_remaining -= mdeplete;

        return true;
    }

    void donate_time_budget(double time_s)
    {
        safe_lock_guard gd(lock);

        for(auto& i : time_budget_remaining)
        {
            for(auto& j : i.second)
            {
                j.second += time_s;

                if(j.second > max_reserve[j.first])
                {
                    j.second = max_reserve[j.first];
                }
            }
        }

        for(auto& i : time_budget_remaining_manual)
        {
            for(auto& j : i.second)
            {
                j.second += time_s;
            }
        }
    }

    /*void donate_time_budget_manual(const std::string& name, double time_s, double mreserve)
    {
        safe_lock_guard gd(lock);

        for(auto& j : time_budget_remaining_manual[name])
        {
            j.second += time_s;

            if(j.second > mreserve)
            {
                j.second = mreserve;
            }
        }
    }*/
};

#define RATELIMIT_DUK(type) if(!get_global_rate_limit()->try_call(get_caller(ctx), rate::type)) {push_error(ctx, "Rate Limit"); return 1;}
#define SHOULD_RATELIMIT(name, type) !get_global_rate_limit()->try_call(name, rate::type)

#define CAN_RUN_ONCE_EVERY(username, ratelimit_name, every) get_global_rate_limit()->try_call_manual(username, ratelimit_name, every, every, every + 1)

#define COOPERATE_KILL_UDATA(udata) sandbox_data* sand_data = (sandbox_data*)udata; \
                                    if(sand_data->terminate_semi_gracefully) \
                                    { printf("Cooperating with kill udata\n");\
                                        throw std::runtime_error("Script ran for more than 5000ms and was cooperatively terminated");\
                                    }\

#define COOPERATE_KILL() duk_memory_functions mem_funcs_duk; duk_get_memory_functions(ctx, &mem_funcs_duk); \
                         sandbox_data* sand_data = (sandbox_data*)mem_funcs_duk.udata; \
                         if(sand_data->terminate_semi_gracefully) \
                         { printf("Cooperating with kill\n");\
                             throw std::runtime_error("Script ran for more than 5000ms and was cooperatively terminated");\
                         } \
                         if(sand_data->terminate_realtime_gracefully) \
                         { printf("Cooperating with kill realtime\n"); \
                            throw std::runtime_error("Terminated realtime script"); \
                         }

#define COOPERATE_KILL_THREAD_LOCAL() if(*tls_get_should_throw() == 1) { throw std::runtime_error("Script ran for more than 5000ms and was cooperatively terminated"); }
#define COOPERATE_KILL_THREAD_LOCAL_URGENT() if(*tls_get_should_throw() >= 2) { throw std::runtime_error("Script ran for more than 5000ms and was cooperatively terminated (overran significantly)"); }

typedef struct duk_hthread duk_context;

bool is_script_timeout(duk_context* ctx);

extern rate_limit global_rate_limit;

inline
rate_limit* get_global_rate_limit()
{
    return &global_rate_limit;
}

#endif // RATE_LIMITING_HPP_INCLUDED
