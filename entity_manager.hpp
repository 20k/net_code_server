#ifndef ENTITY_MANAGER_HPP_INCLUDED
#define ENTITY_MANAGER_HPP_INCLUDED

#include <vec/vec.hpp>
#include "timestamped_event_queue.hpp"

namespace entity
{
    struct entity
    {
        uint32_t id = -1;
    };

    ///ok so. You cannot queue up the events move -> dock
    ///because that is expected to happen from the javascript scripting side
    ///aka await ship.move(dest); await ship.dock(target);
    struct ship : entity
    {
        event_queue::event_queue<vec3f> position_queue;
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
