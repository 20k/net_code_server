#ifndef SLEEPER_HPP_INCLUDED
#define SLEEPER_HPP_INCLUDED

#include <toolkit/clock.hpp>

struct sleeper
{
    steady_timer clk;
    double awake_ms = 0;
    bool exceeded_awake = false;

    void check_sleep(double max_awake_time_ms, double sleep_time);
    void consume_remaining_time(double max_awake_time_ms, double sleep_time);
    void reset();
};

#endif // SLEEPER_HPP_INCLUDED
