#ifndef PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
#define PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED

#include "user.hpp"
#include "duk_object_functions.hpp"
#include "memory_sandbox.hpp"
#include "rate_limiting.hpp"
#include <ratio>

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
void push_error(duk_context* ctx, const std::string& msg)
{
    push_dukobject(ctx, "ok", false, "msg", msg);
}

inline
void push_success(duk_context* ctx)
{
    push_dukobject(ctx, "ok", true);
}

inline
void push_success(duk_context* ctx, const std::string& msg)
{
    push_dukobject(ctx, "ok", true, "msg", msg);
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
duk_ret_t accts__balance(priv_context& priv_ctx, duk_context* ctx, int sl)
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
duk_ret_t scripts__user(priv_context& priv_ctx, duk_context* ctx, int sl)
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
std::string format_script_names(const std::vector<std::string>& names)
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
duk_ret_t scripts__all(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    int pretty = duk_get_prop_string_as_int(ctx, -1, "pretty");

    mongo_requester request;
    request.set_prop("is_script", 1);

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
        std::string str = format_script_names(names);

        duk_push_string(ctx, str.c_str());
    }
    else
    {
        push_duk_val(ctx, names);
    }

    return 1;
}


inline
duk_ret_t accts_internal_xfer(duk_context* ctx, const std::string& from, const std::string& to, double amount)
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
duk_ret_t accts__xfer_gc_to(priv_context& priv_ctx, duk_context* ctx, int sl)
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

    return accts_internal_xfer(ctx, get_caller(ctx), destination_name, amount);
}

inline
duk_ret_t accts__xfer_gc_to_caller(priv_context& priv_ctx, duk_context* ctx, int sl)
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

    return accts_internal_xfer(ctx, priv_ctx.original_host, destination_name, amount);
}

///this is only valid currently, will need to expand to hardcode in certain folders
inline
duk_ret_t scripts__trust(priv_context& priv_ctx, duk_context* ctx, int sl)
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
duk_ret_t chats__send(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    RATELIMIT_DUK(CHAT);

    std::string channel = duk_safe_get_prop_string(ctx, -1, "channel");
    std::string msg = duk_safe_get_prop_string(ctx, -1, "msg");

    if(channel == "" || msg == "" || channel.size() >= 10 || msg.size() >= 10000)
    {
        push_error(ctx, "Usage: #hs.chats.send({channel:\"<name>\", msg:\"msg\"})");
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

            query.update_in_db(global_prop_ctx, update);
        }
    }

    ///ALARM: ALARM: NEED TO RATE LIMIT URGENTLY

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

        std::string msg = tstr + " " + i.get_prop("channel") + " " + i.get_prop("from") + " "  + i.get_prop("msg");

        str = msg + "\n" + str;
    }

    return str;
}

inline
duk_ret_t chats__recent(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string channel = duk_safe_get_prop_string(ctx, -1, "channel");
    int num = duk_get_prop_string_as_int(ctx, -1, "count");
    bool pretty = duk_get_prop_string_as_int(ctx, -1, "pretty");

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
        push_error(ctx, "Usage: #ms.chats.recent({channel:\"<name>\", count:num, pretty:1})");
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
    REGISTER_FUNCTION_PRIV(accts__balance, 3),
    REGISTER_FUNCTION_PRIV(scripts__get_level, 4),
    REGISTER_FUNCTION_PRIV(accts__xfer_gc_to, 2),
    REGISTER_FUNCTION_PRIV(accts__xfer_gc_to_caller, 4),
    REGISTER_FUNCTION_PRIV(scripts__trust, 4),
    REGISTER_FUNCTION_PRIV(scripts__user, 2),
    REGISTER_FUNCTION_PRIV(scripts__all, 4),
    REGISTER_FUNCTION_PRIV(chats__send, 3),
    REGISTER_FUNCTION_PRIV(chats__recent, 2),
};

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
