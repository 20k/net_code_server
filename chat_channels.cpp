#include "chat_channels.hpp"
#include "mongo.hpp"
#include "time.hpp"
#include "serialisables.hpp"

void chats::say_in_local(const std::string& msg, const std::vector<std::string>& to, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);

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

void chats::say_in_channel(const std::string& msg, const std::string& channel, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);

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

    return true
}

void chats::tell_to(const std::string& msg, const std::string& to, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);

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

void chats::join_channel(const std::string& channel, const std::string& user)
{
    mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);

    chat_channel chan;

    if(!db_disk_load(ctx, chan, channel))
        return false;

    chan.user_list.push_back(user);

    db_disk_overwrite(ctx, chan);

    return true;
}

bool chats::leave_channel(const std::string& channel, const std::string& user)
{
    mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);

    chat_channel chan;

    if(!db_disk_load(ctx, chan, channel))
        return false;

    auto it = chan.user_list.find(user);

    if(it == chan.user_list.end())
        return false;

    chan.user_list.erase(it);

    db_disk_overwrite(ctx, chan);

    return true;
}
