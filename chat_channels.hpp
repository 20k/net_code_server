#ifndef CHAT_CHANNELS_HPP_INCLUDED
#define CHAT_CHANNELS_HPP_INCLUDED

#include <vector>
#include <string>
#include <networking/serialisable_fwd.hpp>

struct chat_channel : serialisable, free_function
{
    std::string channel_name;
    std::string password;
    std::vector<std::string> user_list;
};

#endif // CHAT_CHANNELS_HPP_INCLUDED
