#ifndef ENTITY_MANAGER_HPP_INCLUDED
#define ENTITY_MANAGER_HPP_INCLUDED

#include <vec/vec.hpp>
#include "timestamped_event_queue.hpp"

#define SHIP_SPECS_COUNT 6

namespace entity
{
    enum event_type
    {
        MOVE,
        SYSTEM_RECHARGE,
    };

    struct entity
    {
        uint32_t id = -1;
    };

    //using ship_stats = vec<SHIP_SPECS_COUNT, float>;

    using ship_state = std::array<event_queue::event_stack<float>, SHIP_SPECS_COUNT>;

    ///ok so. You cannot queue up the events move -> dock
    ///because that is expected to happen from the javascript scripting side
    ///aka await ship.move(dest); await ship.dock(target);
    struct ship : entity, serialisable, free_function
    {
        event_queue::event_stack<vec3f> position;

        ship_state system_current;
        std::array<float, SHIP_SPECS_COUNT> system_max;

        uint64_t get_next_event();

        vec3f get_position(uint64_t timestamp);
        std::array<float, SHIP_SPECS_COUNT> get_specs(uint64_t timestamp);

        template<typename T>
        void on_trigger_event(size_t current_timestamp, size_t last_timestamp, T t)
        {
            if(position.events[1].timestamp >= last_timestamp && position.events[1].timestamp < current_timestamp && !position.events[1].fired)
            {
                t(*this, position.events[1], event_type::MOVE);
            }

            for(int i=0; i < (int)system_current.size(); i++)
            {
                if(system_current[i].events[1].timestamp >= last_timestamp && system_current[i].events[1].timestamp < current_timestamp && !system_current[i].events[1].fired)
                {
                    t(*this, system_current[i].events[1], event_type::SYSTEM_RECHARGE);
                }
            }
        }

        template<typename T>
        void add_event(uint64_t current_timestamp, const T& event, event_type type)
        {
            if(type != MOVE)
                throw std::runtime_error("Ship systems don't really exist yet, this function is a placeholder");

            position.interrupt_event(current_timestamp, event);
        }
    };
}

#endif // ENTITY_MANAGER_HPP_INCLUDED
