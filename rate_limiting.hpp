#ifndef RATE_LIMITING_HPP_INCLUDED
#define RATE_LIMITING_HPP_INCLUDED

#include <mutex>

namespace rate
{
    enum rates
    {
        CHAT,
        CASH,
    };
}

using rate_limit_t = rate::rates;

struct rate_limit
{
    ///maps username to a type of rate limit
    std::map<std::string, std::map<rate_limit_t, double>> time_budget_remaining;
    std::map<rate_limit_t, double> max_reserve = {{rate::CHAT, 30}, {rate::CASH, 30}};
    std::map<rate_limit_t, double> budget_deplete = {{rate::CHAT, 1}, {rate::CASH, 1}};

    std::mutex lock;

    bool try_call(const std::string& usr_name, rate_limit_t type)
    {
        std::lock_guard gd(lock);

        if(time_budget_remaining.find(usr_name) == time_budget_remaining.end())
        {
            time_budget_remaining[usr_name] = max_reserve;
        }

        double& num_remaining = time_budget_remaining[usr_name][type];

        if(num_remaining <= 0)
            return false;

        num_remaining -= budget_deplete[type];

        return true;
    }

    void donate_time_budget(double time_s)
    {
        std::lock_guard gd(lock);

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
    }
};

#define RATELIMIT_DUK(type) if(get_global_rate_limit()->try_call(get_caller(ctx), rate::type)) {push_error(ctx, "Rate Limit"); return 1;}

static inline rate_limit global_rate_limit;

inline
rate_limit* get_global_rate_limit()
{
    return &global_rate_limit;
}

#endif // RATE_LIMITING_HPP_INCLUDED
