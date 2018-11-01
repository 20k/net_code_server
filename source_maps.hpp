#ifndef SOURCE_MAPS_HPP_INCLUDED
#define SOURCE_MAPS_HPP_INCLUDED

#include <string>
#include <array>
#include <vector>

struct source_segment
{
    enum type
    {
        column_gen, //The zero-based starting column of the line in the generated code that the segment represents.
        sources_index, //an zero-based index into the “sources” list //an hotel
        line_original, //the zero-based starting line in the original source represented
        column_original, //starting column of the line in the source represented
        name_index, //index into the “names” list associated with this segment
    };

    std::array<int, 5> vals;
};

struct source_line
{
    std::vector<source_segment> segments;
};

struct source_position
{
    int line = 0;
    int column = 0;
};

struct source_map
{
    void decode(const std::string& code_in, const std::string& code_out, const std::string& json_obj);

    source_position map(const source_position& pos);

    ///accepts post transform coordinate
    std::string get_caret_text_of(const source_position& pos);

    std::string original_code;
    std::string parsed_code;

    std::vector<source_line> lines;
};

void source_map_tests();

#endif // SOURCE_MAPS_HPP_INCLUDED
