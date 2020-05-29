#ifndef ENTITY_MANAGER_HPP_INCLUDED
#define ENTITY_MANAGER_HPP_INCLUDED

#include <vec/vec.hpp>
#include "timestamped_event_queue.hpp"

namespace entity
{
    enum event_type
    {
        MOVE,
        SYSTEMS_START,
        SHIELDS = SYSTEMS_START,
        ARMOUR,
        HULL,
        FUEL, ///intra system, power
        WARP_FUEL, ///inter system
        JUMP_FUEL,  ///arbitrary inter system
        SYSTEMS_END,
    };

    struct entity
    {
        uint32_t id = -1;
        uint32_t solar_system_id = -1;
    };

    constexpr int systems_count = SYSTEMS_END - SYSTEMS_START;

    //using ship_stats = vec<SHIP_SPECS_COUNT, float>;

    using ship_state = std::array<event_queue::event_stack<float>, systems_count>;

    ///ok so. You cannot queue up the events move -> dock
    ///because that is expected to happen from the javascript scripting side
    ///aka await ship.move(dest); await ship.dock(target);
    struct ship : entity, serialisable, free_function
    {
        event_queue::event_stack<vec3f> position;

        ship_state system_current;
        std::array<float, systems_count> system_max;

        uint64_t get_next_event();
        uint64_t get_next_event_of(event_type type);
        event_queue::timestamp_event_header& get_header_of(event_type type);

        template<typename T>
        void call_for_type(event_type type, T t)
        {
            if(type == event_type::MOVE)
            {
                t(*this, position.events[1], type);
            }
            else if(type >= event_type::SYSTEMS_START && type < event_type::SYSTEMS_END)
            {
                int idx = (int)type - (int)event_type::SYSTEMS_START;

                t(*this, system_current[idx].events[1], type);
            }
            else
            {
                throw std::runtime_error("Type not supported in call_for_type");
            }
        }

        vec3f get_position(uint64_t timestamp);
        std::array<float, systems_count> get_specs(uint64_t timestamp);

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
                    int type = i + (int)SYSTEMS_START;

                    t(*this, system_current[i].events[1], type);
                }
            }
        }

        template<typename T>
        void add_event(uint64_t current_timestamp, const T& event, event_type type)
        {
            if constexpr(std::is_same_v<T, event_queue::timestamp_event_base<vec3f>>)
            {
                if(type != event_type::MOVE)
                    throw std::runtime_error("Wrong type");

                position.interrupt_event(current_timestamp, event);
            }

            else if constexpr(std::is_same_v<T, event_queue::timestamp_event_base<float>>)
            {
                if(type < event_type::SYSTEMS_START || type >= event_type::SYSTEMS_END)
                    throw std::runtime_error("Wrong time for float");

                system_current[type - event_type::SYSTEMS_START].interrupt_event(current_timestamp, event);
            }

            else
                throw std::runtime_error("Ship systems don't really exist yet, this function is a placeholder");
        }
    };
}

#endif // ENTITY_MANAGER_HPP_INCLUDED
