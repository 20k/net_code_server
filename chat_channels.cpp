#include "chat_channels.hpp"
#include "mongo.hpp"
#include "time.hpp"
#include "serialisables.hpp"

void chats::say_in_local(const std::string& msg, const std::vector<std::string>& to, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);

    chat_channel chan;

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
        return;

    chat_message cmsg;
    cmsg.time_ms = get_wall_time();
    cmsg.originator = from;
    cmsg.msg = msg;
    cmsg.recipient_list = chan.user_list;
    cmsg.sent_to_client.resize(cmsg.recipient_list.size());

    chan.history.push_back(cmsg);

    db_disk_overwrite(ctx, chan);
}

void chats::tell_to(const std::string& msg, const std::string& to, const std::string& from)
{
    mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);

    ///yep. I don't want to talk about it
    std::string channel = "$" + to;

    chat_channel chan;

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
