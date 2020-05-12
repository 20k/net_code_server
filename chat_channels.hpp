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
    std::vector<chat_message> history;
};

namespace chats
{
    void say_in_local(const std::string& msg, const std::vector<std::string>& to, const std::string& from);
    bool say_in_channel(const std::string& msg, const std::string& channel, const std::string& from);
    ///locks
    void tell_to(const std::string& msg, const std::string& to, const std::string& from);
    void create_notif_to(const std::string& msg, const std::string& to);

    bool join_channel(const std::string& channel, const std::string& user);
    bool leave_channel(const std::string& channel, const std::string& user);
    void delete_notifs_for(const std::string& user);
    void leave_channels_for(const std::string& user);

    std::vector<chat_message> get_and_update_chat_msgs_for_user(const std::string& name);
}

#endif // CHAT_CHANNELS_HPP_INCLUDED
