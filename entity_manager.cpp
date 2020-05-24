#include "entity_manager.hpp"
#include <assert.h>

uint64_t entity::ship::get_next_event()
{
    uint64_t next = NEVER_TIMESTAMP;

    next = std::min(next, position.events[1].timestamp);

    for(int i=0; i < (int)system_current.size(); i++)
    {
        next = std::min(next, system_current[i].events[1].timestamp);
    }

    return next;
}

uint64_t entity::ship::get_next_event_of(event_type type)
{
    if(type == event_type::MOVE)
        return position.events[1].timestamp;

    throw std::runtime_error("Unsupported get_next_event_of");
}

event_queue::timestamp_event_header& entity::ship::get_header_of(event_type type)
{
    if(type == event_type::MOVE)
        return position.events[1];

    throw std::runtime_error("Unsupported get_next_event_of");
}

vec3f entity::ship::get_position(uint64_t timestamp)
{
    return position.get(timestamp);
}

std::array<float, SHIP_SPECS_COUNT> entity::ship::get_specs(uint64_t timestamp)
{
    std::array<float, SHIP_SPECS_COUNT> ret;

    for(int i=0; i < (int)ret.size(); i++)
    {
        ret[i] = system_current[i].get(timestamp);
    }

    return ret;
}

namespace
{
    struct timestamp_tester
    {
        timestamp_tester()
        {
            event_queue::event_stack<float> stk;

            stk.init(3);

            stk.interrupt(50, 60, 4);

            float val = stk.get(55);

            assert(val == 3.5);

            stk.interrupt(55, 75, 8);

            float val2 = stk.get(65);

            assert(val2 == 5.75);
        }
    };

    timestamp_tester test;
}
