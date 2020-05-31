#ifndef ENTITY_EVENTS_HPP_INCLUDED
#define ENTITY_EVENTS_HPP_INCLUDED

namespace entity
{
    enum event_type
    {
        SYSTEMS_START,
        SHIELDS = SYSTEMS_START,
        ARMOUR,
        HULL,
        FUEL, ///intra system, power
        WARP_FUEL, ///inter system
        JUMP_FUEL,  ///arbitrary inter system
        AMMO,
        THRUST,
        POWER,
        SYSTEMS_END,
        MOVE = SYSTEMS_END,
    };

    constexpr int systems_count = SYSTEMS_END - SYSTEMS_START;
}



#endif // ENTITY_EVENTS_HPP_INCLUDED
