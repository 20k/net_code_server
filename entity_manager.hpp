#ifndef ENTITY_MANAGER_HPP_INCLUDED
#define ENTITY_MANAGER_HPP_INCLUDED

#include <vec/vec.hpp>
#include "timestamped_event_queue.hpp"

#define SHIP_SPECS_COUNT 6

namespace entity
{
    struct entity
    {
        uint32_t id = -1;
    };

    //using ship_stats = vec<SHIP_SPECS_COUNT, float>;

    using ship_state = std::array<event_queue::event_stack<float>, SHIP_SPECS_COUNT>;

    ///ok so. You cannot queue up the events move -> dock
    ///because that is expected to happen from the javascript scripting side
    ///aka await ship.move(dest); await ship.dock(target);
    struct ship : entity
    {
        event_queue::event_stack<vec3f> position;

        ship_state system_current;
        std::array<float, SHIP_SPECS_COUNT> system_max;
    };

    struct manager
    {

    };

    inline
    manager& get_global_manager()
    {
        static manager m;
        return m;
    }
}

#endif // ENTITY_MANAGER_HPP_INCLUDED
