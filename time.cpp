#include "time.hpp"
#include <chrono>
#include <time.h>
#include <atomic>
#include "command_handler_fiber_backend.hpp"

void time_structure::from_time_ms(size_t time_code_ms)
{
    std::chrono::system_clock::time_point chron(std::chrono::seconds(time_code_ms / 1000));

    time_t tt = std::chrono::system_clock::to_time_t(chron);

    tm utc_tm;

    ///this seems like literally the most pointlessly divergent platform behaviour
    #ifdef __WIN32__
    gmtime_s(&utc_tm, &tt);
    #else
    gmtime_r(&tt, &utc_tm);
    #endif

    /*tm local_tm;
    localtime_r(&tt, &local_tm);*/

    hours = utc_tm.tm_hour;
    minutes = utc_tm.tm_min;
    seconds = utc_tm.tm_sec;
    days = utc_tm.tm_mday;
    months = utc_tm.tm_mon;
    years = utc_tm.tm_year;
}

std::string time_structure::format(int unit)
{
    std::string str = std::to_string(unit);

    if(str.size() == 0)
        return "00";

    if(str.size() == 1)
        return "0" + str;

    return str;
}

size_t get_monotonic_time_ms()
{
    return get_wall_time();
}

///https://stackoverflow.com/questions/16177295/get-time-since-epoch-in-milliseconds-preferably-using-c11-chrono
///thanks stack overflow
size_t get_wall_time()
{
    size_t milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();

    return milliseconds_since_epoch;
}

double get_wall_time_s()
{
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::ratio<1, 1>> duration = now.time_since_epoch();
    double real_time = duration.count();

    return real_time;
}

std::atomic_uint64_t& get_internal_tick()
{
    static std::atomic_uint64_t offset = 2;
    return offset;
}

std::pair<uint64_t, uint64_t> tick::get_timestamps()
{
    uint64_t tick = get_internal_tick();
    uint64_t duration = duration_ms();

    return {tick * duration, (tick - 1) * duration};
}

uint64_t tick::duration_ms()
{
    return 1000;
}

void tick::advance()
{
    get_internal_tick()++;
}

void tick::detach_tick_process()
{
    get_global_fiber_queue().add([]()
    {
        uint64_t current_time = get_wall_time();
        uint64_t diff_time = 0;

        while(1)
        {
            uint64_t next_time = get_wall_time();

            uint64_t diff = next_time - current_time;

            diff_time += diff;

            current_time = next_time;

            if(diff_time >= duration_ms())
            {
                diff_time -= duration_ms();
                tick::advance();
            }

            fiber_sleep(50);
        }
    });
}

