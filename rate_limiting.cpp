#include "rate_limiting.hpp"
#include "memory_sandbox.hpp"
#include <SFML/System/Sleep.hpp>
#include "mongo.hpp"
#include "shared_command_handler_state.hpp"
#include "argument_object.hpp"
#include "command_handler_fiber_backend.hpp"

#ifdef USE_FIBERS
#include <boost/fiber/operations.hpp>
#endif // USE_FIBERS

bool is_script_timeout(js::value_context& vctx)
{
    sandbox_data* sand_data = js::get_sandbox_data<sandbox_data>(vctx);

    return sand_data->terminate_semi_gracefully || sand_data->terminate_realtime_gracefully;
}

void handle_sleep(sandbox_data* dat)
{
    if(dat == nullptr)
        return;

    if(tls_get_holds_lock())
    {
        if(*tls_get_holds_lock() > 0)
            return;
    }

    float fiber_load = fiber_overload_factor();

    int sleep_mult = 1;

    ///need to conditionally throw if we're bad
    if(dat->all_shared)
    {
        sleep_mult = dat->all_shared->live_work_units();

        if(sleep_mult < 1)
            sleep_mult = 1;

        if(dat->all_shared->live_work_units() > 10)
        {
            dat->terminate_realtime_gracefully = true;
            throw std::runtime_error("Too many running scripts (10)");
        }

        if(dat->is_realtime)
        {
            if(dat->all_shared->state.should_terminate_any_realtime)
            {
                dat->terminate_realtime_gracefully = true;
                throw std::runtime_error("Terminated Realtime Script");
            }

            {
                safe_lock_guard guard(dat->all_shared->state.script_data_lock);

                realtime_script_data& sdata = dat->all_shared->state.script_data[dat->realtime_script_id];

                if(sdata.should_terminate_realtime)
                {
                    dat->terminate_realtime_gracefully = true;
                    throw std::runtime_error("Terminated Realtime Script");
                }
            }
        }

        if(dat->is_static)
        {

        }
    }

    if(dat->is_realtime)
    {
        ///I think this is all broken
        double sleep_time = 1;
        double max_to_allowed = (1/4.) / sleep_mult;

        //max_frame_time_ms - max_frame_time_ms * allowed_to_max_ratio = sleep_time;
        //(1 - allowed_to_max) * max_frame_time_ms = 1;
        //max_frame_time_ms = 1 / (1 - allowed_to_max);

        double max_frame_time_ms = sleep_time / (1 - max_to_allowed);
        double max_allowed_frame_time_ms = max_frame_time_ms - sleep_time;

        double current_elapsed_time = dat->clk.restart() * 1000;

        dat->realtime_ms_awake_elapsed += current_elapsed_time;

        if(dat->realtime_ms_awake_elapsed > 100)
            dat->realtime_ms_awake_elapsed = 100;

        if(!is_thread_fiber())
        {
            while(dat->realtime_ms_awake_elapsed >= max_allowed_frame_time_ms)
            {
                sf::sleep(sf::milliseconds(sleep_time));

                dat->clk.restart();
                dat->realtime_ms_awake_elapsed -= max_allowed_frame_time_ms;
            }
        }
        else
        {
            #ifdef USE_FIBERS
            double current_framerate = dat->framerate_limit;

            double frametime = (1/current_framerate) * 1000;

            double allowed_executable_time = (1/4.f) * frametime;
            double sleep_time = (1 - (1/4.f)) * frametime * fiber_load;

            ///so, when frames fire off, they have a big chunk of allowed time
            ///this then gets reduced to 1ms on, 4ms off after they miss the frame budget
            if(!dat->new_frame)
            {
                allowed_executable_time = 1;
            }

            sleep_time = 3 * fiber_load * sleep_mult;

            if(dat->realtime_ms_awake_elapsed > allowed_executable_time)
            {
                double awake_overshoot_frac = (dat->realtime_ms_awake_elapsed - allowed_executable_time) / allowed_executable_time;

                double full_sleep_time = sleep_time + awake_overshoot_frac * 3;

                steady_timer elapsed;
                fiber_sleep(full_sleep_time);

                double real_slept = elapsed.get_elapsed_time_s() * 1000;

                double sleep_slice = real_slept / full_sleep_time;

                dat->realtime_ms_awake_elapsed = -sleep_slice * 1;
                dat->clk.restart();

                dat->new_frame = false;
            }
            #endif // USE_FIBERS
        }
    }

    if(dat->is_static)
    {
        double sleep_time = 1 * sleep_mult;
        double awake_time = 1;

        sleep_time += (sleep_time + awake_time) * (fiber_load - 1);

        dat->ms_awake_elapsed_static += dat->clk.restart() * 1000;

        if(dat->ms_awake_elapsed_static > 100)
            dat->ms_awake_elapsed_static = 100;

        if(!is_thread_fiber())
        {
            while(dat->ms_awake_elapsed_static >= awake_time)
            {
                sf::sleep(sf::milliseconds(sleep_time));
                dat->clk.restart();
                dat->ms_awake_elapsed_static -= awake_time;
            }
        }
        else
        {
            #ifdef USE_FIBERS
            int units = dat->ms_awake_elapsed_static / awake_time;

            if(units > 0)
            {
                int idiff = units * sleep_time;

                steady_timer real_sleep;
                boost::this_fiber::sleep_for(std::chrono::milliseconds(idiff));
                double real_time = real_sleep.get_elapsed_time_s() * 1000;

                dat->ms_awake_elapsed_static -= (real_time / sleep_time) * awake_time;
            }

            dat->ms_awake_elapsed_static = clamp(dat->ms_awake_elapsed_static, -20, 100);
            dat->clk.restart();
            #endif // USE_FIBERS
        }

        double elapsed_ms = dat->full_run_clock.get_elapsed_time_s() * 1000;

        if(elapsed_ms >= dat->max_elapsed_time_ms)
        {
            dat->terminate_semi_gracefully = true;
            throw std::runtime_error("Script ran for more than 5000ms and was cooperatively terminated");
        }
    }

    int val = dat->sleep_for;

    if(val > 0)
    {
        //fiber_sleep(val);
    }

    dat->sleep_for -= val;
}

rate_limit global_rate_limit;
