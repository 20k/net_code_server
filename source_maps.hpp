#ifndef SOURCE_MAPS_HPP_INCLUDED
#define SOURCE_MAPS_HPP_INCLUDED

#include <string>
#include <array>
#include <vector>

struct source_segment
{
    std::array<int, 5> vals;
};

struct source_line
{
    std::vector<source_segment> segments;
};

struct source_map
{
    void decode(const std::string& code_in, const std::string& code_out, const std::string& json_obj);

    std::string original_code;
    std::string parsed_code;

    std::vector<source_line> lines;
};

void source_map_tests();

#endif // SOURCE_MAPS_HPP_INCLUDED
