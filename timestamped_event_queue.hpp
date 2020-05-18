#ifndef TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED
#define TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED

#include <array>

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
        uint64_t abs_time_monotonic_ms = 0;

        QuantityType quantity = QuantityType();
    };

    template<int typeidx, typename QuantityType>
    timestamp_event_base<typeidx, QuantityType>
    interpolate_event_at(const timestamp_event_base<typeidx, QuantityType>& p1, const timestamp_event_base<typeidx, QuantityType>& p2, uint64_t timestamp_ms)
    {
        if(timestamp_ms >= p2.abs_time_monotonic_ms)
            return p2;

        if(timestamp_ms <= p1.abs_time_monotonic_ms)
            return p1;

        __float128 t1 = p1.abs_time_monotonic_ms;
        __float128 t2 = p2.abs_time_monotonic_ms;

        __float128 val = timestamp_ms;

        __float128 fraction = (val - t1) / (t2 - t1);

        timestamp_event_base<typeidx, QuantityType> ret;
        ret.abs_time_monotonic_ms = (uint64_t)((t1 * (1 - fraction)) + (t2 * fraction));
        ret.quantity = p1.quantity * (1 - fraction) + p2.quantity * fraction;

        return ret;
    }

    /*struct spatial_event : timestamp_event_base<0, vec3f>
    {
        type t = type::NONE;
        uint64_t entity_id = -1; ///target
    };

    template<int N>
    struct ship_data
    {
        std::array<float, N> data;
    };*/
}

#endif // TIMESTAMPED_EVENT_QUEUE_HPP_INCLUDED
