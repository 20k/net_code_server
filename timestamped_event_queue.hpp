#ifndef TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED
#define TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED

#include <array>
#include <vec/vec.hpp>
#include "time.hpp"

#define NEVER_TIMESTAMP -1
#define QUANTITY_DELTA_ERROR 0.0001f

namespace event_queue
{
    enum type
    {
        MOVE,
        DOCK,
        NONE,
    };

    template<int typeidx, typename QuantityType>
    struct timestamp_event_base
    {
        uint64_t timestamp = 0;

        QuantityType quantity = QuantityType();

        bool blocker = false;
    };

    template<int typeidx, typename QuantityType>
    timestamp_event_base<typeidx, QuantityType>
    interpolate_event_at(const timestamp_event_base<typeidx, QuantityType>& p1, const timestamp_event_base<typeidx, QuantityType>& p2, uint64_t timestamp_ms)
    {
        if(timestamp_ms >= p2.timestamp)
            return p2;

        if(timestamp_ms <= p1.timestamp)
            return p1;

        __float128 t1 = p1.timestamp;
        __float128 t2 = p2.timestamp;

        __float128 val = timestamp_ms;

        __float128 fraction = (val - t1) / (t2 - t1);

        timestamp_event_base<typeidx, QuantityType> ret;
        ret.timestamp = (uint64_t)((t1 * (1 - fraction)) + (t2 * fraction));
        ret.quantity = p1.quantity * (1 - fraction) + p2.quantity * fraction;

        return ret;
    }

    struct spatial_event : timestamp_event_base<0, vec3f>
    {
        ///i think we need two queues
        //type t = type::NONE;
        //uint64_t entity_id = -1; ///target
    };

    template<int N>
    using ship_data = vec<N, float>;

    struct ship_event : timestamp_event_base<1, ship_data>
    {

    };

    template<typename T.
    struct event_queue
    {
        std::vector<T> events;

        void interrupt_with_action(uint64_t current_timestamp, uint64_t interrupt_when, const T& finish)
        {
            if(interrupt_when < current_timestamp)
                throw std::runtime_error("Tried to interrupt into the past");

            int queue_size = events.size();
            uint64_t check_timestamp = interrupt_when;

            for(int i=0; i < queue_size; i++)
            {
                uint64_t current_timestamp = events[i].timestamp;

                ///if my event is earlier than the current event in the queue
                if(check_timestamp < current_timestamp)
                {
                    if(i != 0)
                    {
                        ///event must be later than the previous timestamp, and earlier than the current timestamp
                        assert(check_timestamp < current_timestamp && check_timestamp >= events[i-1].timestamp);

                        auto value = interpolate_event_at(events[i-1], events[i], check_timestamp);

                        ///delete all events in the future, including the current one
                        events.resize(i);
                        events.push_back(value);
                        events.push_back(finish);
                    }

                    else
                    {
                        events.clear();
                        events.push_back(finish);
                    }

                    cleanup_older_than(current_timestamp);

                    return;
                }
            }

            if(queue_size == 0)
            {
                events.push_back(in);
            }
            else
            {
                auto dup = events.back();

                dup.timestamp = interrupt_when;

                events.push_back(dup);
                events.push_back(finish);
            }

            cleanup_older_than(current_timestamp);
        }

        ///if the queue started with > 2 elements, it'll always remain with ~2 elements
        void cleanup_older_than(uint64_t timestamp)
        {
            if(events.size() < 2)
                return;

            int queue_size = events.size();

            for(int i=0; i < queue_size; i++)
            {
                uint64_t current_timestamp = events[i].timestamp;

                ///my timestamp is earlier than the current timestamp in the queue
                if(timestamp < current_timestamp)
                {
                    if(i == 0)
                        return;

                    auto value = interpolate_event_at(events[i-1], events[i], timestamp);

                    events.erase(events.begin(), events.begin() + i);
                    events.insert(events.begin(), value);

                    return;
                }
            }
        }
    };

    /*template<typename T>
    struct event_queue
    {
        std::vector<T> events;

        void insert_specific_event(const T& in)
        {
            if(events.size() == 0)
            {
                events.push_back(in);
                return;
            }

            if(events.size() == 1)
            {
                if(events[0].timestamp <= in.timestamp)
                {
                    events.push_back(in);
                    return;
                }
                else
                {
                    events.insert(events.begin(), in);
                    return;
                }
            }

            int queue_size = events.size();
            uint64_t check_timestamp = in.timestamp;

            for(int i=0; i < queue_size - 1; i++)
            {
                uint64_t current_timestamp = events[i].timestamp;
                uint64_t next_timestamp = events[i+1].timestamp;

                if(check_timestamp < current_timestamp)
                {
                    events.insert(events.begin(), in);
                    return;
                }

                if(check_timestamp >= current_timestamp && check_timestamp < next_timestamp)
                {
                    events.insert(events.begin() + i + 1, in);
                    return;
                }
            }

            events.push_back(in);
        }

        void queue_specific_event_and_drop_future_events(const T& in)
        {
            int queue_size = events.size();
            uint64_t check_timestamp = in.timestamp;

            for(int i=0; i < queue_size; i++)
            {
                uint64_t current_timestamp = events[i].timestamp;

                ///if my event is earlier than the current event in the queue
                if(check_timestamp < current_timestamp)
                {
                    if(i != 0)
                    {
                        ///event must be later than the previous timestamp, and earlier than the current timestamp
                        assert(check_timestamp < current_timestamp && check_timestamp >= events[i-1].timestamp);

                        auto value = interpolate_event_at(events[i-1], events[i], check_timestamp);

                        ///delete all events in the future, including the current one
                        events.resize(i);
                        events.push_back(value);
                        events.push_back(in);
                    }

                    else
                    {
                        events.clear();
                        events.push_back(in);
                    }

                    return;
                }
            }

            events.push_back(in);
        }

        void queue_event_in(const T& in, uint64_t timestamp, uint64_t offset)
        {
            T next = in;
            next.timestamp = timestamp + offset;

            queue_specific_event_and_drop_future_events(next);
        }
    };*/

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
                float difference = max_quantity - current_quantity;
                float time_offset_s = difference / deltatime_s;

                return start_timestamp + time_offset_s * 1000.f;
            }
        }
    }
}

#endif // TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED
