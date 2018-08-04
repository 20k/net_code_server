#ifndef TIMESTAMPED_POSITION_HPP_INCLUDED
#define TIMESTAMPED_POSITION_HPP_INCLUDED

#include <vec/vec.hpp>

#include <json/json.hpp>

namespace timestamped_move_type
{
    enum timestamped_move_type
    {
        MOVE,
        ACTIVATE,
    };
}

using timestamped_move_t = timestamped_move_type::timestamped_move_type;

struct timestamped_position
{
    timestamped_move_t type = timestamped_move_type::MOVE;

    ///general data
    size_t timestamp = 0;
    vec3f position;
    std::string notif_on_finish;

    ///data for activate
    std::string system_to_arrive_at;

    bool is_move() const
    {
        return type == timestamped_move_type::MOVE;
    }

    bool is_activate() const
    {
        return type == timestamped_move_type::ACTIVATE;
    }

    bool is_blocking() const
    {
        return is_activate();
    }

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

        __float128 fraction = (val - t1) / (t2 - t1);

        timestamped_position ret;
        ret.timestamp = (size_t)((t1 * (1 - fraction)) + (t2 * fraction));
        ret.position = p1.position * (1.f - fraction) + p2.position * fraction;

        return ret;
    }
};

inline
void to_json(nlohmann::json& j, const timestamped_position& p)
{
    j = nlohmann::json{
        {"tp", p.type},
        {"ts", p.timestamp},
        {"x", p.position.x()},
        {"y", p.position.y()},
        {"z", p.position.z()},
        {"nt", p.notif_on_finish},
        {"sys", p.system_to_arrive_at},
        };
}

inline
void from_json(const json& j, timestamped_position& p)
{
    try
    {
        p.type = j.at("tp");
    }
    catch(...)
    {

    }

    p.timestamp = j.at("ts");

    p.position.x() = j.at("x");
    p.position.y() = j.at("y");
    p.position.z() = j.at("z");

    ///sigh lack of schema
    try
    {
        p.notif_on_finish = j.at("nt");
    }
    catch(...)
    {

    }

    ///sigh lack of schema
    try
    {
        p.system_to_arrive_at = j.at("sys");
    }
    catch(...)
    {

    }
}

struct timestamp_move_queue
{
    std::vector<timestamped_position> timestamp_queue;

    timestamped_position get_position_at(size_t timestamp) const
    {
        if(timestamp_queue.size() == 0)
        {
            printf("Warning in timestamped move queue\n");
            return timestamped_position();
        }

        if(timestamp_queue.size() == 1)
        {
            if(timestamp_queue[0].is_blocking())
            {
                printf("warning 2 in timestamped move queue\n");
                return timestamped_position();
            }

            return timestamp_queue[0];
        }

        for(int i=1; i < (int)timestamp_queue.size(); i++)
        {
            if(timestamp_queue[i].is_blocking() && timestamp >= timestamp_queue[i-1].timestamp)
            {
                return timestamp_queue[i-1];
            }
        }

        for(int i=0; i < (int)timestamp_queue.size() - 1; i++)
        {
            const timestamped_position& p1 = timestamp_queue[i];
            const timestamped_position& p2 = timestamp_queue[i+1];

            if(p2.is_blocking())
                return p1;

            if(timestamp >= p1.timestamp && timestamp < p2.timestamp)
            {
                return timestamped_position::get_position_at(p1, p2, timestamp);
            }

            if(i + 1 == (int)timestamp_queue.size() - 1)
            {
                if(timestamp >= p2.timestamp)
                    return p2;
            }
        }

        printf("Warning 3 in timestamped move queue\n");

        return timestamped_position::get_position_at(timestamp_queue[(int)timestamp_queue.size() - 2],
                                                     timestamp_queue[(int)timestamp_queue.size() - 1],
                                                     timestamp);
    }

    void add_queue_element(timestamped_position& pos)
    {
        timestamp_queue.push_back(pos);
    }

    void add_activate_element(size_t timestamp, const std::string& system_to_arrive_at)
    {
        timestamped_position tpos;
        tpos.type = timestamped_move_type::ACTIVATE;
        tpos.system_to_arrive_at = system_to_arrive_at;
        tpos.timestamp = timestamp;

        timestamp_queue.push_back(tpos);
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
    j["q"] = p.timestamp_queue;
}

inline
void from_json(const json& j, timestamp_move_queue& p)
{
    p.timestamp_queue = j.at("q").get<std::vector<timestamped_position>>();
}

#endif // TIMESTAMPED_POSITION_HPP_INCLUDED
