#include "privileged_core_scripts.hpp"

#include <ratio>

std::string prettify_chat_strings(std::vector<mongo_requester>& found)
{
    std::string str;

    ///STD::CHRONO PLS
    for(mongo_requester& i : found)
    {
        int64_t time_code_ms = i.get_prop_as_integer("time_ms");

        std::chrono::system_clock::time_point chron(std::chrono::seconds(time_code_ms / 1000));

        typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<24> >::type> days;

        std::chrono::system_clock::duration tp = chron.time_since_epoch();
        days d = std::chrono::duration_cast<days>(tp);
        tp -= d;
        std::chrono::hours h = std::chrono::duration_cast<std::chrono::hours>(tp);
        tp -= h;
        std::chrono::minutes m = std::chrono::duration_cast<std::chrono::minutes>(tp);
        tp -= m;
        std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(tp);
        tp -= s;

        int hour = h.count() % 24;
        int minute = m.count() % 60;

        std::string tstr = "`b" + format_tim(std::to_string(hour)) + format_tim(std::to_string(minute)) + "`";

        std::string msg = tstr + " `P" + i.get_prop("channel") + "`" + " " + colour_string(i.get_prop("user")) + " "  + i.get_prop("msg");

        str = msg + "\n" + str;
    }

    return str;
}

size_t get_wall_time()
{
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> duration = now.time_since_epoch();
    size_t real_time = duration.count();

    return real_time;
}

double get_wall_time_s()
{
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::ratio<1, 1>> duration = now.time_since_epoch();
    double real_time = duration.count();

    return real_time;
}
