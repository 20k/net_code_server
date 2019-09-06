#include "rate_limiting.hpp"
#include "memory_sandbox.hpp"
#include <SFML/System/Sleep.hpp>
#include "mongo.hpp"
#include "shared_command_handler_state.hpp"

bool is_script_timeout(duk_context* ctx)
{
    duk_memory_functions mem_funcs_duk;
    duk_get_memory_functions(ctx, &mem_funcs_duk);
    sandbox_data* sand_data = (sandbox_data*)mem_funcs_duk.udata;

    return sand_data->terminate_semi_gracefully || sand_data->terminate_realtime_gracefully;
}

void handle_sleep(sandbox_data* dat)
{
    if(tls_get_holds_lock())
    {
        if(*tls_get_holds_lock() > 0)
            return;
    }

    int sleep_mult = 1;

    ///need to conditionally throw if we're bad
    if(dat->all_shared)
    {
        sleep_mult = dat->all_shared->live_work_units();

        if(sleep_mult < 1)
            sleep_mult = 1;

        if(dat->all_shared->live_work_units() > 10)
            throw std::runtime_error("Too many running scripts (10)");

        if(dat->is_realtime)
        {
            if(dat->all_shared->state.should_terminate_any_realtime)
                throw std::runtime_error("Terminated Realtime Script");

            {
                safe_lock_guard guard(dat->all_shared->state.lock);

                if(dat->all_shared->state.should_terminate_realtime[dat->realtime_script_id])
                    throw std::runtime_error("Terminated Realtime Script");
            }
        }

        if(dat->is_static)
        {

        }
    }

    if(dat->is_realtime)
    {
        double sleep_time = 1;
        double max_to_allowed = (1/4.) / sleep_mult;

        //max_frame_time_ms - max_frame_time_ms * allowed_to_max_ratio = sleep_time;
        //(1 - allowed_to_max) * max_frame_time_ms = 1;
        //max_frame_time_ms = 1 / (1 - allowed_to_max);

        double max_frame_time_ms = sleep_time / (1 - max_to_allowed);
        double max_allowed_frame_time_ms = max_frame_time_ms - sleep_time;

        dat->realtime_ms_awake_elapsed += dat->clk.restart().asMicroseconds() / 1000.;

        if(dat->realtime_ms_awake_elapsed > 100)
            dat->realtime_ms_awake_elapsed = 100;

        while(dat->realtime_ms_awake_elapsed >= max_allowed_frame_time_ms)
        {
            sf::sleep(sf::milliseconds(sleep_time));

            dat->clk.restart();
            dat->realtime_ms_awake_elapsed -= max_allowed_frame_time_ms;
        }
    }

    if(dat->is_static)
    {
        double sleep_time = 1 * sleep_mult;
        double awake_time = 1;

        dat->ms_awake_elapsed_static += dat->clk.restart().asMicroseconds() / 1000.;

        if(dat->ms_awake_elapsed_static > 100)
            dat->ms_awake_elapsed_static = 100;

        while(dat->ms_awake_elapsed_static >= awake_time)
        {
            sf::sleep(sf::milliseconds(sleep_time));
            dat->clk.restart();
            dat->ms_awake_elapsed_static -= 1;
        }

        double elapsed_ms = dat->full_run_clock.getElapsedTime().asMicroseconds() / 1000.;

        if(elapsed_ms >= dat->max_elapsed_time_ms)
        {
            throw std::runtime_error("Script ran for more than 5000ms and was cooperatively terminated");
        }
    }

    int val = dat->sleep_for;

    if(val > 0)
    {
        /*double diff = 0;

        sf::Clock clk;

        while(diff < val)
        {
            sf::sleep(sf::milliseconds(1));

            diff += clk.restart().asMicroseconds() / 1000.;
        }

        dat->sleep_for -= floor(diff);*/

        sf::sleep(sf::milliseconds(val));
    }

    dat->sleep_for -= val;
}

rate_limit global_rate_limit;
