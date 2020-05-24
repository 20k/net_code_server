#ifndef REALTIME_SCRIPT_DATA_HPP_INCLUDED
#define REALTIME_SCRIPT_DATA_HPP_INCLUDED

#include <vector>
#include <cstdint>
#include "../../entity_manager.hpp"

///does not need to be thread safe
struct realtime_script_data
{
    std::vector<uint32_t> entity_ids;
    std::vector<uint64_t> event_timestamps;
    std::vector<entity::event_type> event_types;
};

#endif // REALTIME_SCRIPT_DATA_HPP_INCLUDED
