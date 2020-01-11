#ifndef SHARED_DATA_HPP_INCLUDED
#define SHARED_DATA_HPP_INCLUDED

#include <atomic>
#include <deque>
#include <string>
#include <mutex>
#include <shared_mutex>

struct shared_data
{
    std::deque<std::string> read_queue;
    std::deque<std::string> write_queue;
    std::string user;

    std::shared_mutex read_lock;
    std::shared_mutex write_lock;
    std::shared_mutex user_lock;

    bool has_front_read()
    {
        std::shared_lock<std::shared_mutex> lk(read_lock);

        return read_queue.size() > 0;
    }

    bool has_front_write()
    {
        std::shared_lock<std::shared_mutex> lk(write_lock);

        return write_queue.size() > 0;
    }

    std::string get_front_read()
    {
        std::unique_lock<std::shared_mutex> lk(read_lock);

        std::string ret = read_queue.front();

        read_queue.pop_front();

        return ret;
    }

    std::string get_front_write()
    {
        std::unique_lock<std::shared_mutex> lk(write_lock);

        std::string ret = write_queue.front();

        write_queue.pop_front();

        return ret;
    }

    void add_back_write(const std::string& str)
    {
        std::unique_lock<std::shared_mutex> lk(write_lock);

        write_queue.push_back(str);
    }

    void add_back_read(const std::string& str)
    {
        std::unique_lock<std::shared_mutex> lk(read_lock);

        read_queue.push_back(str);
    }

    void set_user(const std::string& in)
    {
        std::unique_lock<std::shared_mutex> lk(user_lock);

        user = in;
    }

    std::string get_user()
    {
        std::shared_lock<std::shared_mutex> lk(user_lock);

        return user;
    }
};

#endif // SHARED_DATA_HPP_INCLUDED
