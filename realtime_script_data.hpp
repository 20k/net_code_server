#ifndef REALTIME_SCRIPT_DATA_HPP_INCLUDED
#define REALTIME_SCRIPT_DATA_HPP_INCLUDED

#include <vector>
#include <cstdint>

///does not need to be thread safe
struct realtime_script_data
{
    std::vector<uint32_t> entity_ids;
    std::vector<uint64_t> event_timestamps;
};

#endif // REALTIME_SCRIPT_DATA_HPP_INCLUDED
