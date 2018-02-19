#ifndef HTTP_BEAST_SERVER_HPP_INCLUDED
#define HTTP_BEAST_SERVER_HPP_INCLUDED

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <mutex>

struct global_state
{
    std::map<std::string, int> auth_locks;
    int64_t global_id = 1;
    std::mutex auth_lock;
};

void http_test_run();

#endif // HTTP_BEAST_SERVER_HPP_INCLUDED
