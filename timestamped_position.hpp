#ifndef TIMESTAMPED_POSITION_HPP_INCLUDED
#define TIMESTAMPED_POSITION_HPP_INCLUDED

#include <vec/vec.hpp>

#include <json/json.hpp>

struct timestamped_position
{
    size_t timestamp = 0;
    vec3f position;

    static
    timestamped_position get_position_at(const timestamped_position& p1, const timestamped_position& p2, size_t timestamp)
    {
        if(timestamp > p2.timestamp)
            return p2;

        if(timestamp < p1.timestamp)
            return p1;

        if(p2.timestamp == p1.timestamp)
            return p2;

        if(p2.position == p1.position)
            return p2;

        __float128 t1 = p1.timestamp;
        __float128 t2 = p2.timestamp;

        __float128 val = timestamp;

        __float128 fraction = (t1 - val) / (t2 - t1);

        timestamped_position ret;
        ret.timestamp = (size_t)((t1 * (1 - fraction)) + (t2 * fraction));
        ret.position = p1.position * (1.f - fraction) + p2.position * fraction;

        return ret;
    }
};

inline
void to_json(nlohmann::json& j, const timestamped_position& p)
{
    j = nlohmann::json{{"x", p.position.x()}, {"y", p.position.y()}, {"z", p.position.z()}, {"ts", p.timestamp}};
}

inline
void from_json(const json& j, timestamped_position& p)
{
    p.timestamp = j.at("ts");

    p.position.x() = j.at("x");
    p.position.y() = j.at("y");
    p.position.z() = j.at("z");
}

struct timestamp_move_queue
{
    std::vector<timestamped_position> timestamp_queue;

    timestamped_position get_position_at(size_t timestamp) const
    {
        if(timestamp_queue.size() == 0)
            return timestamped_position();

        if(timestamp_queue.size() == 1)
            return timestamp_queue[0];

        for(int i=0; i < (int)timestamp_queue.size() - 1; i++)
        {
            const timestamped_position& p1 = timestamp_queue[0];
            const timestamped_position& p2 = timestamp_queue[1];

            if(timestamp >= p1.timestamp && timestamp < p2.timestamp)
            {
                return timestamped_position::get_position_at(p1, p2, timestamp);
            }
        }

        return timestamped_position::get_position_at(timestamp_queue[(int)timestamp_queue.size() - 2],
                                                     timestamp_queue[(int)timestamp_queue.size() - 1],
                                                     timestamp);
    }

    void add_queue_element(timestamped_position& pos)
    {
        timestamp_queue.push_back(pos);
    }

    void cleanup_old_elements(size_t timestamp)
    {
        int cleanup_to = 0;

        for(int i=0; i < (int)timestamp_queue.size() - 1; i++)
        {
            ///both me and my next door neighbour are < time, means i'm out of date
            if(timestamp_queue[i].timestamp < timestamp && timestamp_queue[i+1].timestamp < timestamp)
                cleanup_to = i;
        }

        for(int i=0; i < cleanup_to && (int)timestamp_queue.size() != 1; i++)
        {
            timestamp_queue.erase(timestamp_queue.begin());
        }
    }
};


inline
void to_json(nlohmann::json& j, const timestamp_move_queue& p)
{
    j = nlohmann::json{{"q", p.timestamp_queue}};
}

inline
void from_json(const json& j, timestamp_move_queue& p)
{
    p.timestamp_queue = j.at("q").get<std::vector<timestamped_position>>();
}

#endif // TIMESTAMPED_POSITION_HPP_INCLUDED
