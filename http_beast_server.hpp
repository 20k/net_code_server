#ifndef HTTP_BEAST_SERVER_HPP_INCLUDED
#define HTTP_BEAST_SERVER_HPP_INCLUDED

#include <atomic>

struct global_state
{
    //std::map<std::string, int> auth_locks;
    std::atomic_int global_id = 1;
    //std::mutex auth_lock;
};

void boot_connection_handlers();

#endif // HTTP_BEAST_SERVER_HPP_INCLUDED
