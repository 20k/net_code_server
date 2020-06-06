#include "entity_manager.hpp"
#include <assert.h>
#include "entity_events.hpp"

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
    else if(type >= SYSTEMS_START && type < SYSTEMS_END)
        return system_current[type - SYSTEMS_START].events[1].timestamp;
    else
        throw std::runtime_error("Unsupported get_next_event_of");
}

event_queue::timestamp_event_header& entity::ship::get_header_of(event_type type)
{
    if(type == event_type::MOVE)
        return position.events[1];
    else if(type >= SYSTEMS_START && type < SYSTEMS_END)
        return system_current[type - SYSTEMS_START].events[1];
    else
        throw std::runtime_error("Unsupported get_header_of");
}

vec3f entity::ship::get_position(uint64_t timestamp)
{
    return position.get(timestamp);
}

std::array<float, entity::systems_count> entity::ship::get_specs(uint64_t timestamp)
{
    std::array<float, systems_count> ret;

    for(int i=0; i < (int)ret.size(); i++)
    {
        ret[i] = system_current[i].get(timestamp);
    }

    return ret;
}

bool entity::is_valid_ship_construction(const std::vector<int>& ids)
{
    int power_net = 0;
    int control_net = 0;
    int size_total = 0;

    for(int id : ids)
    {
        component::base comp = component::get_base_by_id(id);

        power_net += comp.produced_ps[::entity::POWER];
        control_net += comp.produced_ps[::entity::CONTROL];
        size_total += comp.size;
    }

    return power_net >= 0 && control_net > 0 && size_total <= 14;
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
