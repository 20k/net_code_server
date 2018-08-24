#include "time.hpp"
#include <chrono>

void time_structure::from_time_ms(size_t time_code_ms)
{
    std::chrono::system_clock::time_point chron(std::chrono::seconds(time_code_ms / 1000));

    typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<24> >::type> chrono_days;

    std::chrono::system_clock::duration tp = chron.time_since_epoch();
    chrono_days d = std::chrono::duration_cast<chrono_days>(tp);
    tp -= d;
    std::chrono::hours h = std::chrono::duration_cast<std::chrono::hours>(tp);
    tp -= h;
    std::chrono::minutes m = std::chrono::duration_cast<std::chrono::minutes>(tp);
    tp -= m;
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(tp);
    tp -= s;

    int hour = h.count() % 24;
    int minute = m.count() % 60;
    int second = s.count() % 60;
    int day = d.count();

    hours = hour;
    minutes = minute;
    seconds = second;
    days = day;
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
