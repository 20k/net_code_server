#include "chat_channels.hpp"
#include "mongo.hpp"
#include "time.hpp"
#include "serialisables.hpp"
#include <libncclient/nc_util.hpp>

void chats::say_in_local(const std::string& msg, const std::vector<std::string>& to, const std::string& from)
{
    chat_message cmsg;

    {
        mongo_nolock_proxy nctx = get_global_mongo_chat_messages_context(-2);

        cmsg.id = db_storage_backend::get_unique_id();
        cmsg.time_ms = get_wall_time();
        cmsg.originator = from;
        cmsg.msg = msg;
        cmsg.recipient_list = to;
        cmsg.sent_to_client.resize(cmsg.recipient_list.size());

        db_disk_overwrite(nctx, cmsg);
    }

    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    chat_channel chan;
    chan.channel_name = "$$local";

    db_disk_load(ctx, chan, "$$local");
    chan.history.push_back(cmsg.id);

    db_disk_overwrite(ctx, chan);
}

bool chats::say_in_channel(const std::string& msg, const std::string& channel, const std::string& from)
{
    std::vector<std::string> user_list;

    {
        mongo_nolock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

        chat_channel chan;

        if(!db_disk_load(ctx, chan, channel))
            return false;

        user_list = chan.user_list;
    }

    chat_message cmsg;

    {
        mongo_nolock_proxy nctx = get_global_mongo_chat_messages_context(-2);

        cmsg.id = db_storage_backend::get_unique_id();
        cmsg.time_ms = get_wall_time();
        cmsg.originator = from;
        cmsg.msg = msg;
        cmsg.recipient_list = user_list;
        cmsg.sent_to_client.resize(cmsg.recipient_list.size());

        db_disk_overwrite(nctx, cmsg);
    }

    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    chat_channel chan;

    if(!db_disk_load(ctx, chan, channel))
        return false;

    chan.history.push_back(cmsg.id);

    db_disk_overwrite(ctx, chan);

    return true;
}

void chats::tell_to(const std::string& msg, const std::string& to, const std::string& from)
{
    chat_message cmsg;

    {
        mongo_nolock_proxy nctx = get_global_mongo_chat_messages_context(-2);

        cmsg.id = db_storage_backend::get_unique_id();
        cmsg.time_ms = get_wall_time();
        cmsg.originator = from;
        cmsg.msg = msg;
        cmsg.recipient_list = {to};
        cmsg.sent_to_client.resize(1);

        db_disk_overwrite(nctx, cmsg);
    }

    mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

    ///yep. I don't want to talk about it
    std::string channel = "$" + to;

    chat_channel chan;
    chan.channel_name = channel;

    ///will create if it doesn't exist, tells are a virtual channel
    db_disk_load(ctx, chan, channel);
    chan.history.push_back(cmsg.id);

    db_disk_overwrite(ctx, chan);
}

void chats::create_notif_to(const std::string& msg, const std::string& to)
{
    chats::tell_to(msg, "^" + to, "$core");
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
    std::vector<chat_channel> all_chans;

    {
        mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);
        all_chans = db_disk_load_all(ctx, chat_channel());
    }

    for(chat_channel& i : all_chans)
    {
        for(auto it = i.history.begin(); it != i.history.end();)
        {
            mongo_nolock_proxy nctx = get_global_mongo_chat_messages_context(-2);

            chat_message msg;
            db_disk_load(nctx, msg, *it);

            auto it2 = std::find(msg.recipient_list.begin(), msg.recipient_list.end(), user);

            if(it2 == msg.recipient_list.end())
            {
                it++;
                continue;
            }

            int offset = std::distance(msg.recipient_list.begin(), it2);

            msg.recipient_list.erase(it2);
            msg.sent_to_client.erase(msg.sent_to_client.begin() + offset);

            db_disk_overwrite(nctx, msg);

            /*if(msg.recipient_list.size() == 0)
            {
                it = i.history.erase(it);
            }
            else
            {
                it++;
            }*/

            it++;
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

std::vector<std::string> chats::get_channels_for_user(const std::string& name)
{
    std::vector<chat_channel> all_channels;
    std::vector<std::string> ret;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(-2);

        all_channels = db_disk_load_all(mongo_ctx, chat_channel());
    }

    for(auto& i : all_channels)
    {
        for(auto& k : i.user_list)
        {
            if(k == name)
            {
                ret.push_back(i.channel_name);
                break;
            }
        }
    }

    return ret;
}

/*std::vector<nlohmann::json> get_and_update_chat_msgs_for_user(user& usr)
{
    std::vector<nlohmann::json> found;

    usr.cleanup_call_stack(-2);

    {
        mongo_nolock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
        ctx.change_collection(usr.get_call_stack().back());

        nlohmann::json to_send;
        to_send["is_chat"] = 1;
        to_send["processed"] = 0;

        found = fetch_from_db(ctx, to_send);

        nlohmann::json old_search = to_send;

        to_send["processed"] = 1;

        update_in_db_if_exact(ctx, old_search, to_send);
    }

    if(found.size() > 1000)
        found.resize(1000);

    return found;
}*/

bool is_chat_channel(const std::string& name)
{
    if(name.size() == 0)
        return false;

    if(name == "$$local")
        return true;

    if(name[0] == '$')
        return false;

    if(name[0] == '^')
        return false;

    return true;
}

bool is_tell_channel(const std::string& name)
{
    if(name.size() == 0)
        return false;

    if(name == "$$local")
        return false;

    return name[0] == '$';
}

bool is_notif_channel(const std::string& name)
{
    if(name.size() == 0)
        return false;

    return name[0] == '^';
}

std::vector<std::pair<std::string, chat_message>> chats::get_and_update_chat_msgs_for_user(const std::string& name)
{
    std::vector<std::pair<std::string, chat_message>> ret;
    std::vector<chat_channel> all_channels;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(-2);

        all_channels = db_disk_load_all(mongo_ctx, chat_channel());
    }

    for(chat_channel& chan : all_channels)
    {
        if(!is_chat_channel(chan.channel_name))
            continue;

        for(size_t id : chan.history)
        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_messages_context(-2);

            chat_message msg;
            db_disk_load(mongo_ctx, msg, id);

            auto it = std::find(msg.recipient_list.begin(), msg.recipient_list.end(), name);

            if(it == msg.recipient_list.end())
                continue;

            int offset = std::distance(msg.recipient_list.begin(), it);

            if(msg.sent_to_client[offset])
                continue;

            msg.sent_to_client[offset] = 1;

            ret.push_back({chan.channel_name, msg});

            db_disk_overwrite(mongo_ctx, msg);
        }
    }

    return ret;
}

std::vector<chat_message> chats::get_and_update_tells_for_user(const std::string& name)
{
    std::vector<chat_message> ret;
    std::vector<chat_channel> all_channels;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(-2);
        all_channels = db_disk_load_all(mongo_ctx, chat_channel());
    }

    for(chat_channel& chan : all_channels)
    {
        if(!is_tell_channel(chan.channel_name))
            continue;

        for(size_t id : chan.history)
        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_messages_context(-2);

            chat_message msg;
            db_disk_load(mongo_ctx, msg, id);

            auto it = std::find(msg.recipient_list.begin(), msg.recipient_list.end(), name);

            if(it == msg.recipient_list.end())
                continue;

            int offset = std::distance(msg.recipient_list.begin(), it);

            if(msg.sent_to_client[offset])
                continue;

            msg.sent_to_client[offset] = 1;

            ret.push_back(msg);
            db_disk_overwrite(mongo_ctx, msg);
        }
    }

    return ret;
}

std::vector<chat_message> chats::get_and_update_notifs_for_user(const std::string& name)
{
    std::vector<chat_message> ret;
    std::vector<chat_channel> all_channels;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(-2);
        all_channels = db_disk_load_all(mongo_ctx, chat_channel());
    }

    for(chat_channel& chan : all_channels)
    {
        if(!is_notif_channel(chan.channel_name))
            continue;

        for(size_t id : chan.history)
        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_messages_context(-2);

            chat_message msg;
            db_disk_load(mongo_ctx, msg, id);

            auto it = std::find(msg.recipient_list.begin(), msg.recipient_list.end(), name);

            if(it == msg.recipient_list.end())
                continue;

            int offset = std::distance(msg.recipient_list.begin(), it);

            if(msg.sent_to_client[offset])
                continue;

            msg.sent_to_client[offset] = 1;

            ret.push_back(msg);
            db_disk_overwrite(mongo_ctx, msg);
        }
    }

    return ret;
}

void chats::strip_all_old()
{

}

namespace
{
std::string format_time(const std::string& in)
{
    if(in.size() == 1)
        return "0" + in;

    if(in.size() == 0)
        return "00";

    return in;
}
}

std::string chats::prettify(const std::vector<chat_message>& in, bool use_channels, const std::string& channel_name)
{
    std::string str;

    ///STD::CHRONO PLS
    for(int kk=0; kk < (int)in.size(); kk++)
    {
        const chat_message& fmsg = in[kk];

        size_t time_code_ms = fmsg.time_ms;

        std::string channel = channel_name;
        std::string usrname = fmsg.originator;
        std::string msg = fmsg.msg;

        time_structure time_s;
        time_s.from_time_ms(time_code_ms);

        int hour = time_s.hours;
        int minute = time_s.minutes;

        std::string tstr = "`b" + format_time(std::to_string(hour)) + format_time(std::to_string(minute)) + "`";

        std::string chan_str = " `P" + channel + "`";

        std::string total_msg;

        if(use_channels)
            total_msg = tstr + chan_str + " " + colour_string(usrname) + " "  + msg;
        else
            total_msg = tstr + " " + colour_string(usrname) + " "  + msg;

        if(kk != 0)
            str = total_msg + "\n" + str;
        else
            str = total_msg;
    }

    return str;
}
