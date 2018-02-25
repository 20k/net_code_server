#ifndef PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
#define PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED

#include "user.hpp"
#include "duk_object_functions.hpp"
#include "memory_sandbox.hpp"
#include "rate_limiting.hpp"
#include <ratio>
#include "auth.hpp"
#include "item.hpp"

#define COOPERATE_KILL() duk_memory_functions mem_funcs_duk; duk_get_memory_functions(ctx, &mem_funcs_duk); \
                         sandbox_data* sand_data = (sandbox_data*)mem_funcs_duk.udata; \
                         if(sand_data->terminate_semi_gracefully) \
                         { printf("Cooperating with kill\n");\
                             while(1){Sleep(10);}\
                         }

struct priv_context
{
    ///if we execute accts.balance from i20k.hello, this is i20k not accts.balance
    std::string original_host;

    priv_context(const std::string& ohost) : original_host(ohost)
    {

    }
};

using function_priv_t = duk_ret_t (*)(priv_context&, duk_context*, int);

inline
bool can_run(int csec_level, int maximum_sec)
{
    return csec_level <= maximum_sec;
}

inline
duk_ret_t push_error(duk_context* ctx, const std::string& msg)
{
    push_dukobject(ctx, "ok", false, "msg", msg);
    return 1;
}

inline
duk_ret_t push_success(duk_context* ctx)
{
    push_dukobject(ctx, "ok", true);
    return 1;
}

inline
duk_ret_t push_success(duk_context* ctx, const std::string& msg)
{
    push_dukobject(ctx, "ok", true, "msg", msg);
    return 1;
}

///could potentially use __FUNCTION__ here
///as it should work across msvc/gcc/clang... but... technically not portable
#define SL_GUARD(x) if(!can_run(sl, x)){ push_error(ctx, "Security level guarantee failed"); return 1; }

struct priv_func_info
{
    function_priv_t func;
    int sec_level = 0;
};

extern
std::map<std::string, priv_func_info> privileged_functions;

///so say this is midsec
///we can run if the sl is midsec or lower
///lower sls are less secure

///hmm. Maybe we want to keep sls somewhere which is dynamically editable like global properties in the db
///cache the calls, and like, refresh the cache every 100 calls or something
inline
duk_ret_t cash__balance(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(get_thread_id(ctx));

    user usr;
    usr.load_from_db(mongo_user_info, get_caller(ctx));

    //std::string cash_string = std::to_string((int64_t)usr.cash);

    double cash_val = usr.cash;
    push_duk_val(ctx, cash_val);

    return 1;
}

inline
duk_ret_t scripts__get_level(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    ///so we have an object
    ///of the form name:whatever
    ///really need a way to parse these out from duktape

    duk_get_prop_string(ctx, -1, "name");

    if(!duk_is_string(ctx, -1))
    {
        push_error(ctx, "Call with name:\"scriptname\"");
        return 1;
    }

    std::string str = duk_get_string(ctx, -1);

    duk_pop(ctx);

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

    script_info script;
    //script.load_from_disk_with_db_metadata(str);
    script.name = str;
    script.load_from_db(mongo_ctx);

    if(privileged_functions.find(str) != privileged_functions.end())
    {
        duk_push_int(ctx, privileged_functions[str].sec_level);
        return 1;
    }

    if(!script.valid)
    {
        push_error(ctx, "Invalid script name " + str);
        return 1;
    }

    duk_push_int(ctx, script.seclevel);

    return 1;
}

inline
duk_ret_t scripts__me(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string usr = get_caller(ctx);

    mongo_requester request;
    request.set_prop("owner", usr);
    request.set_prop("is_script", 1);

    mongo_lock_proxy item_context = get_global_mongo_user_items_context(get_thread_id(ctx));

    std::vector<mongo_requester> results = request.fetch_from_db(item_context);

    std::vector<std::string> names;

    for(mongo_requester& req : results)
    {
        if(!req.has_prop("item_id"))
            continue;

        names.push_back(req.get_prop("item_id"));
    }

    push_duk_val(ctx, names);

    return 1;
}

inline
std::string format_pretty_names(const std::vector<std::string>& names)
{
    std::string ret;

    for(auto& i : names)
    {
        ret.append(i);
        ret += "\n";
    }

    return ret;
}

///should take a pretty:1 argument
inline
duk_ret_t scripts__public(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    int pretty = !duk_get_prop_string_as_int(ctx, -1, "array");
    int seclevel = duk_get_prop_string_as_int(ctx, -1, "sec", -1);

    mongo_requester request;
    request.set_prop("is_script", 1);
    request.set_prop("in_public", 1);

    if(seclevel >= 0 && seclevel <= 4)
        request.set_prop("seclevel", seclevel);

    request.set_prop_sort_on("item_id", 1);

    ///seclevel
    //request.set_prop("seclevel", num);
    //request.set_prop("in_public", "1"); ///TODO: FOR WHEN YOU CAN UP PUBLIC

    mongo_lock_proxy item_context = get_global_mongo_user_items_context(get_thread_id(ctx));

    std::vector<mongo_requester> results = request.fetch_from_db(item_context);

    std::vector<std::string> names;

    for(mongo_requester& req : results)
    {
        names.push_back(req.get_prop("item_id"));
    }

    if(pretty)
    {
        std::string str = format_pretty_names(names);

        duk_push_string(ctx, str.c_str());
    }
    else
    {
        push_duk_val(ctx, names);
    }

    return 1;
}


inline
duk_ret_t cash_internal_xfer(duk_context* ctx, const std::string& from, const std::string& to, double amount)
{
    COOPERATE_KILL();

    if(round(amount) != amount || amount < 0 || amount >= pow(2, 32))
    {
        push_error(ctx, "Amount error");
        return 1;
    }

    if(from == to)
    {
        push_error(ctx, "Money definitely shifted hands");
        return 1;
    }

    ///NEED TO LOCK MONGODB HERE

    mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(get_thread_id(ctx));

    user destination_usr;

    if(!destination_usr.load_from_db(mongo_user_info, to))
    {
        push_error(ctx, "User does not exist");
        return 1;
    }

    user caller_usr;

    if(!caller_usr.load_from_db(mongo_user_info, from))
    {
        push_error(ctx, "From user does not exist");
        return 1;
    }

    double remaining = caller_usr.cash - amount;

    if(remaining < 0)
    {
        push_error(ctx, "Can't send this amount");
        return 1;
    }

    ///need to check destination usr can handle amount

    caller_usr.cash -= amount;
    destination_usr.cash += amount;

    ///hmm so
    ///we'll need to lock db when doing this
    caller_usr.overwrite_user_in_db(mongo_user_info);
    destination_usr.overwrite_user_in_db(mongo_user_info);

    ///NEED TO END MONGODB LOCK HERE

    push_success(ctx);

    return 1;
}

///TODO: TRANSACTION HISTORY
inline
duk_ret_t cash__xfer_to(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    RATELIMIT_DUK(CASH);

    ///need a get either or
    ///so we can support to and name
    duk_get_prop_string(ctx, -1, "to");

    if(!duk_is_string(ctx, -1))
    {
        push_error(ctx, "Call with to:\"usr\"");
        return 1;
    }

    std::string destination_name = duk_get_string(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "amount");

    double amount = 0;

    if(!duk_is_number(ctx, -1))
    {
        push_error(ctx, "Only numbers supported atm");
        return 1;
    }

    amount = duk_get_number(ctx, -1);
    duk_pop(ctx);

    return cash_internal_xfer(ctx, get_caller(ctx), destination_name, amount);
}

inline
duk_ret_t cash__xfer_to_caller(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string destination_name = get_caller(ctx);

    duk_get_prop_string(ctx, -1, "amount");

    double amount = 0;

    if(!duk_is_number(ctx, -1))
    {
        push_error(ctx, "Only numbers supported atm");
        return 1;
    }

    amount = duk_get_number(ctx, -1);
    duk_pop(ctx);

    return cash_internal_xfer(ctx, priv_ctx.original_host, destination_name, amount);
}

///this is only valid currently, will need to expand to hardcode in certain folders
inline
duk_ret_t scripts__core(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::vector<std::string> ret;

    for(auto& i : privileged_functions)
    {
        ret.push_back(i.first);
    }

    push_duk_val(ctx, ret);

    return 1;
}

inline
duk_ret_t msgs__send(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    RATELIMIT_DUK(CHAT);

    std::string channel = duk_safe_get_prop_string(ctx, -1, "channel");
    std::string msg = duk_safe_get_prop_string(ctx, -1, "msg");

    if(channel == "" || msg == "" || channel.size() >= 10 || msg.size() >= 10000)
    {
        push_error(ctx, "Usage: #hs.msg.send({channel:\"<name>\", msg:\"msg\"})");
        return 1;
    }

    int64_t global_id = 0;

    {
        mongo_lock_proxy global_prop_ctx = get_global_mongo_global_properties_context(get_thread_id(ctx));

        mongo_requester request;
        request.set_prop("chats_send_is_gid", 1);

        std::vector<mongo_requester> found = request.fetch_from_db(global_prop_ctx);

        if(found.size() == 0)
        {
            mongo_requester gid;
            gid.set_prop("chats_send_is_gid", 1);
            gid.set_prop("chats_send_gid", 1);

            gid.insert_in_db(global_prop_ctx);
        }
        else
        {
            mongo_requester& cur = found[0];

            global_id = cur.get_prop_as_integer("chats_send_gid");

            //std::cout << "gid " << global_id << std::endl;

            mongo_requester query;
            query.set_prop("chats_send_is_gid", 1);

            mongo_requester update;
            update.set_prop("chats_send_gid", global_id + 1);

            query.update_in_db_if_exists(global_prop_ctx, update);
        }
    }

    mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channels_context(get_thread_id(ctx));
    //mongo_ctx->change_collection(channel);

    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> duration = now.time_since_epoch();
    size_t real_time = duration.count();

    mongo_requester request;
    request.set_prop("channel", channel);
    request.set_prop("msg", msg);
    request.set_prop("time_ms", real_time);
    request.set_prop("from", get_caller(ctx));
    request.set_prop_int("uid", global_id);

    request.insert_in_db(mongo_ctx);

    push_success(ctx);

    return 1;
}

inline
std::string format_tim(const std::string& in)
{
    if(in.size() == 1)
        return "0" + in;

    if(in.size() == 0)
        return "00";

    return in;
}

inline
std::string prettify_chat_strings(std::vector<mongo_requester>& found)
{
    std::string str;

    ///STD::CHRONO PLS
    for(mongo_requester& i : found)
    {
        int64_t time_code_ms = i.get_prop_as_integer("time_ms");

        std::chrono::system_clock::time_point chron(std::chrono::seconds(time_code_ms / 1000));

        typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<24> >::type> days;

        std::chrono::system_clock::duration tp = chron.time_since_epoch();
        days d = std::chrono::duration_cast<days>(tp);
        tp -= d;
        std::chrono::hours h = std::chrono::duration_cast<std::chrono::hours>(tp);
        tp -= h;
        std::chrono::minutes m = std::chrono::duration_cast<std::chrono::minutes>(tp);
        tp -= m;
        std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(tp);
        tp -= s;

        int hour = h.count() % 24;
        int minute = m.count() % 60;

        std::string tstr = "`b" + format_tim(std::to_string(hour)) + format_tim(std::to_string(minute)) + "`";

        std::string msg = tstr + " `P" + i.get_prop("channel") + "`" + " " + colour_string(i.get_prop("from")) + " "  + i.get_prop("msg");

        str = msg + "\n" + str;
    }

    return str;
}

inline
duk_ret_t msgs__recent(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string channel = duk_safe_get_prop_string(ctx, -1, "channel");
    int num = duk_get_prop_string_as_int(ctx, -1, "count");
    bool pretty = !duk_get_prop_string_as_int(ctx, -1, "array");

    /*int offset = 0;

    if(duk_has_prop_string(ctx, -1, "offset"))
    {
        offset = duk_get_prop_string_as_int(ctx, -1, "offset");
    }*/

    if(num <= 0)
        num = 10;

    if(channel.size() == 0)
        channel = "0000";

    std::cout << "fchannel " << channel << std::endl;

    if(channel == "" || num >= 100 || channel.size() >= 10)
    {
        push_error(ctx, "Usage: #ms.msg.recent({channel:\"<name>\", count:num, pretty:1})");
        return 1;
    }

    mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channels_context(get_thread_id(ctx));
    //mongo_ctx->change_collection(channel);

    ///ALARM: ALARM: RATE LIMIT

    mongo_requester request;
    request.set_prop("channel", channel);
    request.set_prop_sort_on("uid", -1);

    request.set_limit(num);

    std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

    if(!pretty)
    {
        duk_push_array(ctx);

        int cur_count = 0;
        for(mongo_requester& i : found)
        {
            duk_push_object(ctx);

            for(auto& kk : i.properties)
            {
                std::string key = kk.first;
                std::string value = kk.second;

                put_duk_keyvalue(ctx, key, value);
            }

            duk_put_prop_index(ctx, -2, cur_count);

            cur_count++;
        }
    }
    else
    {
        std::string str = prettify_chat_strings(found);

        push_duk_val(ctx, str);
    }

    return 1;
}

inline
duk_ret_t users__me(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int pretty = !duk_get_prop_string_as_int(ctx, -1, "array", 0);

    std::string caller = get_caller(ctx);

    user current_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!current_user.exists(mongo_ctx, caller))
        {
            push_error(ctx, "Yeah you really broke something here");
            return 1;
        }

        current_user.load_from_db(mongo_ctx, caller);
    }

    mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(get_thread_id(ctx));

    std::string auth_token = current_user.auth;

    auth user_auth;
    user_auth.load_from_db(mongo_ctx, auth_token);

    std::vector names = user_auth.users;

    ///users in user db don't know about the other users
    ///and we can't perform a query across multiple collections, quite rightly
    ///so have to revisit updating auth
    if(pretty)
    {
        std::string str = format_pretty_names(names);

        push_duk_val(ctx, str);

        return 1;
    }
    else
    {
        push_duk_val(ctx, names);

        return 1;
    }

    return 1;
}

inline
duk_ret_t items__create(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();
    RATELIMIT_DUK(UPG_CHEAT);

    item test_item;

    int item_type = duk_get_prop_string_as_int(ctx, -1, "type", 2);

    if(item_type < 0 || item_type >= item_types::ERR)
    {
        push_error(ctx, "type: 0 to 7");
        return 1;
    }

    test_item = item_types::get_default_of((item_types::item_type)item_type);

    ///this isn't adequate
    ///we need a give item to user, and remove item from user primitive
    ///which sorts out indices
    //test_item.set_prop("owner", get_caller(ctx));

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(get_thread_id(ctx));
        test_item.generate_set_id(mongo_ctx);
    }

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));
        test_item.create_in_db(mongo_ctx);
    }

    if(test_item.transfer_to_user(get_caller(ctx), get_thread_id(ctx)))
        duk_push_int(ctx, test_item.get_prop_as_integer("item_id"));
    else
        push_error(ctx, "Could not transfer item to caller");

    return 1;
}

#if 0
///pretty tired when i wrote this check it for mistakes
inline
duk_ret_t sys__disown_upg(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    item test_item;

    int item_id = duk_get_prop_string_as_int(ctx, -1, "uid", -1);

    if(item_id < 0)
    {
        push_error(ctx, "Invalid");
        return 1;
    }

    test_item.set_prop("item_id", item_id);

    if(test_item.remove_from_user(get_caller(ctx), get_thread_id(ctx)))
        duk_push_int(ctx, test_item.get_prop_as_integer("item_id"));
    else
        push_error(ctx, "Could not remove item from caller");

    return 1;
}

inline
duk_ret_t sys__xfer_upgrade_uid(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    item test_item;

    int item_id = duk_get_prop_string_as_int(ctx, -1, "uid", -1);

    if(item_id < 0)
    {
        push_error(ctx, "Invalid");
        return 1;
    }

    std::string from = get_caller(ctx);
    std::string to = duk_safe_get_prop_string(ctx, -1, "to");

    test_item.set_prop("item_id", item_id);

    if(test_item.transfer_from_to(from, to, get_thread_id(ctx)))
        duk_push_int(ctx, test_item.get_prop_as_integer("item_id"));
     else
        push_error(ctx, "Could not xfer");

    return 1;
}
#endif // 0

inline
std::string format_item(item& i, bool is_short, user& usr)
{
    if(is_short)
        return i.get_prop("short_name");

    std::string ret = "{\n";

    bool is_open_source = i.get_prop_as_integer("open_source");

    for(auto& p : i.properties)
    {
        if(!is_open_source && p.first == "unparsed_source")
            continue;

        ret += "    " + p.first + ": " + p.second + ",\n";
    }

    if(usr.has_loaded_item(i.get_prop("item_id")))
        ret += "    loaded: true\n";

    return ret + "}";
}

inline
duk_object_t get_item_raw(item& i, bool is_short, user& usr)
{
    duk_object_t obj;

    if(usr.has_loaded_item(i.get_prop("item_id")))
        obj["loaded"] = true;

    if(is_short)
    {
        obj["short_name"] = i.get_prop("short_name");

        return obj;
    }

    bool is_open_source = i.get_prop_as_integer("open_source");

    for(auto& p : i.properties)
    {
        if(!is_open_source && p.first == "unparsed_source")
            continue;

        if(is_short && p.first != "short_name")
            continue;

        obj[p.first] = p.second;
    }

    return obj;
}

inline
duk_ret_t items__manage(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int pretty = !duk_get_prop_string_as_int(ctx, -1, "array", 0);
    int full = duk_get_prop_string_as_int(ctx, -1, "full", 0);

    int load_idx = duk_get_prop_string_as_int(ctx, -1, "load", -1);
    int unload_idx = duk_get_prop_string_as_int(ctx, -1, "unload", -1);

    user found_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));
        mongo_ctx->change_collection(get_caller(ctx));

        found_user.load_from_db(mongo_ctx, get_caller(ctx));

        if(!found_user.valid)
        {
            push_error(ctx, "No such user/really catastrophic error");
            return 1;
        }

        if(load_idx >= 0 || unload_idx >= 0)
        {
            std::string tl = found_user.index_to_item(load_idx);
            std::string tul = found_user.index_to_item(unload_idx);

            ///NEED TO CHECK CONSTRAINTS HERE ALARM
            found_user.load_item(tl);
            found_user.unload_item(tul);

            found_user.overwrite_user_in_db(mongo_ctx);

            push_success(ctx);

            return 1;
        }
    }

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

    std::vector<std::string> to_ret = str_to_array(found_user.upgr_idx);

    if(pretty)
    {
        std::string formatted;

        if(full)
           formatted = "[\n";

        int idx = 0;

        for(std::string& item_id : to_ret)
        {
            item next;
            next.load_from_db(mongo_ctx, item_id);

            if(!full)
            {
                if(found_user.has_loaded_item(next.get_prop("item_id")))
                    formatted += "`D" + std::to_string(idx) + "`: ";
                else
                    formatted += std::to_string(idx) + ": ";
            }

            formatted += format_item(next, !full, found_user);// + ",\n";

            if(full)
            {
                if(idx != (int)to_ret.size() - 1)
                    formatted += ",\n";
                else
                    formatted += "\n";
            }
            else
            {
                if(idx != (int)to_ret.size() - 1)
                    formatted += "\n";
            }

            idx++;
        }

        if(full)
            formatted += "]";

        push_duk_val(ctx, formatted);
    }
    else
    {
        std::vector<duk_object_t> objs;

        for(std::string& item_id : to_ret)
        {
            item next;
            next.load_from_db(mongo_ctx, item_id);

            objs.push_back(get_item_raw(next, !full, found_user));
        }

        push_duk_val(ctx, objs);
    }

    return 1;
}

inline
duk_ret_t items__xfer_to(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);

    if(item_idx < 0)
    {
        push_error(ctx, "Invalid index");
        return 1;
    }

    std::string from = get_caller(ctx);
    std::string to = duk_safe_get_prop_string(ctx, -1, "to");

    item placeholder;

    if(placeholder.transfer_from_to_by_index(item_idx, from, to, get_thread_id(ctx)))
        duk_push_int(ctx, placeholder.get_prop_as_integer("item_id"));
     else
        push_error(ctx, "Could not xfer");

    return 1;
}

inline
duk_ret_t items__bundle_script(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);
    std::string scriptname = duk_safe_get_prop_string(ctx, -1, "name");

    if(scriptname == "")
        return push_error(ctx, "Invalid name");

    std::string full_script_name = get_caller(ctx) + "." + scriptname;

    if(!is_valid_full_name_string(full_script_name))
        return push_error(ctx, "Invalid name");

    if(item_idx < 0)
        return push_error(ctx, "Invalid index");

    user current_user;

    ///hmm. I really need a generic double locking system
    ///ALARM: NOT SAFE WITHOUT DOUBLE LOCKING DOUBLE LOCKIGN ISNT SAFE YET
    {
        mongo_lock_proxy user_lock = get_global_mongo_user_info_context(get_thread_id(ctx));
        user_lock->change_collection(get_caller(ctx));

        current_user.load_from_db(user_lock, get_caller(ctx));
    }

    std::string item_id = current_user.index_to_item(item_idx);

    if(item_id == "")
        return push_error(ctx, "Invalid index");

    {
        mongo_lock_proxy item_lock = get_global_mongo_user_items_context(get_thread_id(ctx));

        item found_bundle;

        if(!found_bundle.exists_in_db(item_lock, item_id))
            return push_error(ctx, "No such item");

        found_bundle.load_from_db(item_lock, item_id);

        if(found_bundle.get_prop("item_type") != std::to_string(item_types::EMPTY_SCRIPT_BUNDLE))
            return push_error(ctx, "Not a script bundle");

        if(found_bundle.get_prop("full") != "0")
            return push_error(ctx, "Not an empty script bundle");

        item found_script;

        if(!found_bundle.exists_in_db(item_lock, full_script_name))
            return push_error(ctx, "No such script");

        found_script.load_from_db(item_lock, full_script_name);

        int max_storage = found_bundle.get_prop_as_integer("max_script_size");

        if((int)found_script.get_prop("unparsed_source").size() > max_storage)
            return push_error(ctx, "Empty bundle does not contain enough space");

        /*script_info found_script;
        found_script.name = full_script_name;

        if(!found_script.load_from_db(item_lock))
            return push_error(ctx, "No such script");

        int max_storage = found_bundle.get_prop_as_integer("max_script_size");

        if(!found_script.unparsed_source.size() >= max_storage)
            return push_error(ctx, "Empty bundle does not contain enough space");*/

        found_bundle.set_prop("unparsed_source", found_script.get_prop("unparsed_source"));
        found_bundle.set_prop("full", 1);

        found_bundle.overwrite_in_db(item_lock);
    }

    return push_success(ctx);
}

inline
duk_ret_t items__register_bundle(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);
    std::string scriptname = duk_safe_get_prop_string(ctx, -1, "name");

    if(scriptname == "")
        return push_error(ctx, "Invalid name");

    std::string full_script_name = get_caller(ctx) + "." + scriptname;

    if(!is_valid_full_name_string(full_script_name))
        return push_error(ctx, "Invalid name");

    if(item_idx < 0)
        return push_error(ctx, "Invalid index");

    user current_user;

    {
        mongo_lock_proxy user_lock = get_global_mongo_user_info_context(get_thread_id(ctx));
        user_lock->change_collection(get_caller(ctx));

        current_user.load_from_db(user_lock, get_caller(ctx));
    }

    std::string item_id = current_user.index_to_item(item_idx);

    if(item_id == "")
        return push_error(ctx, "Invalid index");

    if(!current_user.has_loaded_item(item_id))
        return push_error(ctx, "Item not loaded");

    {
        mongo_lock_proxy item_lock = get_global_mongo_user_items_context(get_thread_id(ctx));

        item found_bundle;

        if(!found_bundle.exists_in_db(item_lock, item_id))
            return push_error(ctx, "No such item");

        found_bundle.load_from_db(item_lock, item_id);

        if(found_bundle.get_prop("item_type") != std::to_string(item_types::EMPTY_SCRIPT_BUNDLE))
            return push_error(ctx, "Not a script bundle");

        if(found_bundle.get_prop("full") != "1")
            return push_error(ctx, "Not a full script bundle");

        found_bundle.set_prop("registered_as", scriptname);

        found_bundle.overwrite_in_db(item_lock);
    }

    return push_success(ctx);
}

inline
std::string parse_function_hack(std::string in)
{
    int len = in.size();

    for(int i=0; i < len-1; i++)
    {
        if(in[i] == '_' && in[i+1] == '_')
        {
            in[i] = '.';
            in[i+1] = '$';
        }
    }

    in.erase(std::remove(in.begin(), in.end(), '$'), in.end());

    return in;
}

#define REGISTER_FUNCTION_PRIV(x, y) {parse_function_hack(#x), {&x, y}}

inline
std::map<std::string, priv_func_info> privileged_functions
{
    REGISTER_FUNCTION_PRIV(cash__balance, 3),
    REGISTER_FUNCTION_PRIV(cash__xfer_to, 2),
    REGISTER_FUNCTION_PRIV(cash__xfer_to_caller, 4),
    REGISTER_FUNCTION_PRIV(scripts__get_level, 4),
    REGISTER_FUNCTION_PRIV(scripts__core, 4),
    REGISTER_FUNCTION_PRIV(scripts__me, 2),
    REGISTER_FUNCTION_PRIV(scripts__public, 4),
    REGISTER_FUNCTION_PRIV(msgs__send, 3),
    REGISTER_FUNCTION_PRIV(msgs__recent, 2),
    REGISTER_FUNCTION_PRIV(users__me, 0),
    REGISTER_FUNCTION_PRIV(items__create, 0),
    //REGISTER_FUNCTION_PRIV(sys__disown_upg, 0),
    //REGISTER_FUNCTION_PRIV(sys__xfer_upgrade_uid, 0),
    REGISTER_FUNCTION_PRIV(items__xfer_to, 1),
    REGISTER_FUNCTION_PRIV(items__manage, 2),
    REGISTER_FUNCTION_PRIV(items__bundle_script, 1),
    REGISTER_FUNCTION_PRIV(items__register_bundle, 0),
};

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
