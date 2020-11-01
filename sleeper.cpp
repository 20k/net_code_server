#include "sleeper.hpp"
#include "command_handler_fiber_backend.hpp"

void sleeper::check_sleep(double max_awake_time_ms, double sleep_time)
{
    awake_ms += clk.get_elapsed_time_s() * 1000;

    if(awake_ms >= max_awake_time_ms)
    {
        //double overshot_fraction = (awake_ms - max_awake_time_ms) / max_awake_time_ms;
        double overshot_awake_amount = awake_ms - max_awake_time_ms;

        double extra_sleep = overshot_awake_amount * sleep_time / max_awake_time_ms;

        double total_sleep = extra_sleep + sleep_time;

        //std::cout << "sleep for " << total_sleep << std::endl;
        //std::cout << "awake for " << awake_ms << " max " << max_awake_time_ms << std::endl;

        steady_timer elapsed;
        fiber_sleep(total_sleep);

        double real_slept = elapsed.get_elapsed_time_s() * 1000;

        double slept_extra = real_slept - total_sleep;

        double awake_sub = -(slept_extra / sleep_time) * max_awake_time_ms;

        //awake_ms = 0;
        awake_ms = awake_sub;

        exceeded_awake = true;
    }

    clk.restart();
}

void sleeper::consume_remaining_time(double max_awake_time_ms, double sleep_time)
{
    double time_to_end_of_allowed = max_awake_time_ms - awake_ms;

    double total_sleep = time_to_end_of_allowed + sleep_time;

    steady_timer oversleep;
    fiber_sleep(total_sleep);

    double elapsed_sleep = oversleep.get_elapsed_time_s() * 1000.;

    awake_ms = -((elapsed_sleep - total_sleep) / total_sleep) * max_awake_time_ms;

    clk.restart();
}

void sleeper::reset()
{
    exceeded_awake = false;
    clk.restart();
}
