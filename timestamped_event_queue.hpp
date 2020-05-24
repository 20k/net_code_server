#ifndef TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED
#define TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED

#include <array>
#include <vec/vec.hpp>
#include "time.hpp"
#include <networking/serialisable_fwd.hpp>

///something that will or should never happen
#define NEVER_TIMESTAMP -1
///something that has always been true
#define ALWAYS_TIMESTAMP 0
#define QUANTITY_DELTA_ERROR 0.0001f

namespace event_queue
{
    enum type
    {
        MOVE,
        DOCK,
        NONE,
    };

    struct timestamp_event_header
    {
        uint64_t timestamp = NEVER_TIMESTAMP;
        std::string originator_script;
        uint32_t originator_script_id = -1;
        uint32_t entity_id = -1;
        std::string callback;
        bool fired = true;
    };

    template<typename QuantityType>
    struct timestamp_event_base : timestamp_event_header, serialisable, free_function
    {
        QuantityType quantity = QuantityType();
    };

    ///guaranteed that the return value with have a timestamp of timestamp_ms
    template<typename QuantityType>
    timestamp_event_base<QuantityType>
    interpolate_event_at(const timestamp_event_base<QuantityType>& p1, const timestamp_event_base<QuantityType>& p2, uint64_t timestamp_ms)
    {
        if(timestamp_ms >= p2.timestamp)
        {
            auto rval = p2;
            rval.timestamp = timestamp_ms;
            return rval;
        }

        if(timestamp_ms <= p1.timestamp)
        {
            auto rval = p1;
            rval.timestamp = timestamp_ms;
            return rval;
        }

        __float128 t1 = p1.timestamp;
        __float128 t2 = p2.timestamp;

        __float128 val = timestamp_ms;

        __float128 fraction = (val - t1) / (t2 - t1);

        timestamp_event_base<QuantityType> ret = p2;
        ret.timestamp = timestamp_ms;
        ret.quantity = p1.quantity * (1 - fraction) + p2.quantity * fraction;

        return ret;
    }

    template<typename T>
    struct event_stack : serialisable, free_function
    {
        ///when a event_stack is made, it starts with the array [initial_event, initial_event]
        std::array<timestamp_event_base<T>, 2> events;

        void init(const T& val)
        {
            timestamp_event_base<T> base;
            base.quantity = val;
            base.timestamp = ALWAYS_TIMESTAMP;

            events[0] = base;
            events[1] = base;
        }

        void interrupt(uint64_t current_timestamp, uint64_t finish_timestamp, const T& finish)
        {
            if(current_timestamp < events[0].timestamp)
                throw std::runtime_error("cannot be earlier than our start timestamp");

            else if(current_timestamp >= events[0].timestamp && current_timestamp < events[1].timestamp)
            {
                ///get current value in time
                auto value = interpolate_event_at(events[0], events[1], current_timestamp);

                events[0] = value;

                timestamp_event_base<T> next = events[1];
                next.quantity = finish;
                next.timestamp = finish_timestamp;

                events[1] = next;
            }

            ///ok so. these two branches are obviously exactly the same
            ///but this is a non obvious corner case, so its explicitly signalled
            else if(current_timestamp >= events[1].timestamp)
            {
                auto value = interpolate_event_at(events[0], events[1], current_timestamp);

                timestamp_event_base<T> next = events[1];
                next.quantity = finish;
                next.timestamp = finish_timestamp;

                events[0] = value;
                events[1] = next;
            }
        }

        void interrupt_event(uint64_t current_timestamp, const timestamp_event_base<T>& finish)
        {
            if(current_timestamp < events[0].timestamp)
                throw std::runtime_error("cannot be earlier than our start timestamp");

            else if(current_timestamp >= events[0].timestamp && current_timestamp < events[1].timestamp)
            {
                ///get current value in time
                auto value = interpolate_event_at(events[0], events[1], current_timestamp);

                events[0] = value;
                events[1] = finish;
            }

            ///ok so. these two branches are obviously exactly the same
            ///but this is a non obvious corner case, so its explicitly signalled
            else if(current_timestamp >= events[1].timestamp)
            {
                auto value = interpolate_event_at(events[0], events[1], current_timestamp);

                events[0] = value;
                events[1] = finish;
            }
        }

        T get(uint64_t timestamp)
        {
            return interpolate_event_at(events[0], events[1], timestamp).quantity;
        }

        /*void instantaneous_change(const T& value)
        {
            timestamp_event_base<T> rval;
            rval.quantity = value;
            rval.timestamp = ALWAYS_TIMESTAMP;

            events[0] = rval;
            events[1] = rval;
        }*/
    };

    inline
    uint64_t calculate_end_timestamp(uint64_t start_timestamp, float current_quantity, float end_quantity, float deltatime_s)
    {
        if(end_quantity == current_quantity)
            return start_timestamp;

        float fabs_deltatime = fabs(deltatime_s);
        float fabs_difference = fabs(end_quantity - current_quantity);

        if(fabs_deltatime < QUANTITY_DELTA_ERROR)
        {
            if(fabs_difference < QUANTITY_DELTA_ERROR)
                return start_timestamp + 1; ///this is the numerically unstable part, which is why this redundant check is in here
            else
                return start_timestamp + 1;
        }
        else
        {
            if(fabs_difference < QUANTITY_DELTA_ERROR)
                return NEVER_TIMESTAMP;
            else
            {
                float difference = end_quantity - current_quantity;
                float time_offset_s = difference / deltatime_s;

                return start_timestamp + time_offset_s * 1000.f;
            }
        }
    }
}

#endif // TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED
