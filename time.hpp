#ifndef TIME_HPP_INCLUDED
#define TIME_HPP_INCLUDED

#include <stddef.h>
#include <string>

struct time_structure
{
    int years = 0;
    int months = 0;
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;

    void from_time_ms(size_t time_point_ms);

    static std::string format(int unit);
};

size_t get_wall_time();
double get_wall_time_s();

#endif // TIME_HPP_INCLUDED
