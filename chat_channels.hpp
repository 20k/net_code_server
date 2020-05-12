#ifndef CHAT_CHANNELS_HPP_INCLUDED
#define CHAT_CHANNELS_HPP_INCLUDED

#include <vector>
#include <string>
#include <networking/serialisable_fwd.hpp>

struct chat_message : serialisable, free_function
{
    size_t time_ms = 0;
    std::string originator;
    std::string msg;
    std::vector<std::string> recipient_list;
    std::vector<int> sent_to_client;
};

struct chat_channel : serialisable, free_function
{
    std::string channel_name;
    std::string password;
    std::vector<std::string> user_list;
};

#endif // CHAT_CHANNELS_HPP_INCLUDED
