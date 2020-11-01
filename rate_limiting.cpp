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
        double current_framerate = dat->framerate_limit;

        double frametime = (1/current_framerate) * 1000;

        double allowed_executable_time = (1/4.f) * frametime;

        ///so, when frames fire off, they have a big chunk of allowed time
        ///this then gets reduced to 1ms on, 4ms off after they miss the frame budget
        if(dat->sleep_realtime.exceeded_awake)
        {
            allowed_executable_time = 1;
        }

        ///1:3, 1ms awake + 3ms asleep = 4ms, aka 1/4 awake and 3/4 asleep
        double sleep_time = 3;
        ///one 'frame'
        sleep_time += 4 * (fiber_load - 1);
        sleep_time += 4 * (sleep_mult - 1);

        dat->sleep_realtime.check_sleep(allowed_executable_time, sleep_time);
    }

    if(dat->is_static)
    {
        double sleep_time = 1 * sleep_mult;
        double awake_time = 1;

        sleep_time += (sleep_time + awake_time) * (fiber_load - 1);

        dat->sleep_static.check_sleep(awake_time, sleep_time);

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
        fiber_sleep(val);
    }

    dat->sleep_for -= val;
}

rate_limit global_rate_limit;
