#ifndef SOURCE_MAPS_HPP_INCLUDED
#define SOURCE_MAPS_HPP_INCLUDED

#include <string>

struct source_map
{

    void decode(const std::string& str);
};

void source_map_tests();

#endif // SOURCE_MAPS_HPP_INCLUDED
