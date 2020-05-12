#include "chat_channels.hpp"
#include "mongo.hpp"
#include "time.hpp"
#include "serialisables.hpp"

void chats::say_in_local(const std::string& msg, const std::vector<std::string>& to, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    chat_channel chan;
    chan.channel_name = "$$local";

    db_disk_load(ctx, chan, "$$local");

    chat_message cmsg;
    cmsg.time_ms = get_wall_time();
    cmsg.originator = from;
    cmsg.msg = msg;
    cmsg.recipient_list = to;
    cmsg.sent_to_client.resize(cmsg.recipient_list.size());

    chan.history.push_back(cmsg);

    db_disk_overwrite(ctx, chan);
}

bool chats::say_in_channel(const std::string& msg, const std::string& channel, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    chat_channel chan;

    if(!db_disk_load(ctx, chan, channel))
        return false;

    chat_message cmsg;
    cmsg.time_ms = get_wall_time();
    cmsg.originator = from;
    cmsg.msg = msg;
    cmsg.recipient_list = chan.user_list;
    cmsg.sent_to_client.resize(cmsg.recipient_list.size());

    chan.history.push_back(cmsg);

    db_disk_overwrite(ctx, chan);

    return true;
}

void chats::tell_to(const std::string& msg, const std::string& to, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    ///yep. I don't want to talk about it
    std::string channel = "$" + to;

    chat_channel chan;
    chan.channel_name = channel;

    ///will create if it doesn't exist, tells are a virtual channel
    db_disk_load(ctx, chan, channel);

    chat_message cmsg;
    cmsg.time_ms = get_wall_time();
    cmsg.originator = from;
    cmsg.msg = msg;
    cmsg.recipient_list = {to};
    cmsg.sent_to_client.resize(1);

    chan.history.push_back(cmsg);

    db_disk_overwrite(ctx, chan);
}

void chats::create_notif_to(const std::string& msg, const std::string& to)
{
    chats::tell_to(msg, to, "core");
}

bool chats::join_channel(const std::string& channel, const std::string& user)
{
    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    chat_channel chan;

    if(!db_disk_load(ctx, chan, channel))
        return false;

    chan.user_list.push_back(user);

    db_disk_overwrite(ctx, chan);

    return true;
}

bool chats::leave_channel(const std::string& channel, const std::string& user)
{
    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    chat_channel chan;

    if(!db_disk_load(ctx, chan, channel))
        return false;

    auto it = std::find(chan.user_list.begin(), chan.user_list.end(), user);

    if(it == chan.user_list.end())
        return false;

    chan.user_list.erase(it);

    db_disk_overwrite(ctx, chan);

    return true;
}

void chats::delete_notifs_for(const std::string& user)
{
    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    std::vector<chat_channel> all_chans = db_disk_load_all(ctx, chat_channel());

    for(chat_channel& i : all_chans)
    {
        for(auto it = i.history.begin(); it != i.history.end();)
        {
            auto it2 = std::find(it->recipient_list.begin(), it->recipient_list.end(), user);

            if(it2 == it->recipient_list.end())
            {
                it++;
                continue;
            }

            int offset = std::distance(it->recipient_list.begin(), it2);

            it->recipient_list.erase(it2);
            it->sent_to_client.erase(it->sent_to_client.begin() + offset);

            if(it->recipient_list.size() == 0)
            {
                it = i.history.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}

void chats::leave_channels_for(const std::string& user)
{
    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    std::vector<chat_channel> all_chans = db_disk_load_all(ctx, chat_channel());

    for(chat_channel& i : all_chans)
    {
        auto it = std::find(i.user_list.begin(), i.user_list.end(), user);

        if(it == i.user_list.end())
            continue;

        i.user_list.erase(it);
    }
}
