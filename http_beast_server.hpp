#ifndef HTTP_BEAST_SERVER_HPP_INCLUDED
#define HTTP_BEAST_SERVER_HPP_INCLUDED

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <mutex>

struct request
{
    int64_t id = 0;
    std::string data;
};

struct request_holder
{
    std::mutex mut;

    std::deque<request> to_process;
    std::map<int64_t, std::string> send_back;

    void add_request(request& r)
    {
        std::lock_guard<std::mutex> lock(mut);

        to_process.push_back(r);
    }

    request fetch_remove_next()
    {
        std::lock_guard<std::mutex> lock(mut);

        request req = to_process[0];

        to_process.pop_front();

        return req;
    }

    void add_to_send_back_queue(request& r)
    {
        std::lock_guard<std::mutex> lock(mut);

        send_back[r.id] = r.data;
    }

    bool any_requests()
    {
        std::lock_guard<std::mutex> lock(mut);

        return to_process.size() > 0;
    }
};

void http_test_run(request_holder& req);

#endif // HTTP_BEAST_SERVER_HPP_INCLUDED
