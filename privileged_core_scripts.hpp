#ifndef PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
#define PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED

#include "user.hpp"
#include "duk_object_functions.hpp"
#include "memory_sandbox.hpp"
#include "rate_limiting.hpp"
#include "auth.hpp"
#include "item.hpp"
#include "script_util_shared.hpp"

#include <vec/vec.hpp>

#define USE_SECRET_CONTENT
#ifdef USE_SECRET_CONTENT
#include <secret/secret.hpp>
#include <secret/node.hpp>
#include <secret/npc_manager.hpp>
#endif // USE_SECRET_CONTENT

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
    ///for every script other than a loc, this will just be the name of the current function
    ///otherwise its eg i20k.c_f_pi3232 or whatever
    std::string called_as;

    priv_context(const std::string& ohost, const std::string& called_as) : original_host(ohost), called_as(called_as)
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
    bool is_privileged = false; ///can only be called by something privileged
};

extern
std::map<std::string, priv_func_info> privileged_functions;

struct script_arg
{
    std::string key;
    std::string val;
};

inline
duk_ret_t make_logs_on(duk_context* ctx, const std::string& username, user_node_t type, const std::vector<std::string>& logs)
{
    std::string caller = get_caller(ctx);

    user usr;

    {
        mongo_lock_proxy user_info = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!usr.load_from_db(user_info, username))
            return push_error(ctx, "No such user");
    }


    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, username);
        nodes.load_from_db(node_ctx, username);

        user_node* node = nodes.type_to_node(type);

        if(node == nullptr)
            return push_error(ctx, "Error: Red Martian");

        node->logs.insert(node->logs.end(), logs.begin(), logs.end());

        nodes.overwrite_in_db(node_ctx);
    }

    return 0;
}

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

inline
duk_ret_t scripts__me(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    bool make_array = duk_get_prop_string_as_int(ctx, -1, "array");

    std::string usr = get_caller(ctx);

    user loaded_user;

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        loaded_user.load_from_db(user_ctx, usr);
    }

    std::vector<std::string> names;
    mongo_lock_proxy item_context = get_global_mongo_user_items_context(get_thread_id(ctx));

    ///regular scripts
    {
        mongo_requester request;
        request.set_prop("owner", usr);
        request.set_prop("is_script", 1);

        std::vector<mongo_requester> results = request.fetch_from_db(item_context);

        for(mongo_requester& req : results)
        {
            if(!req.has_prop("item_id"))
                continue;

            names.push_back("#" + req.get_prop("item_id"));
        }
    }

    {
        mongo_requester request;
        request.set_prop("owner", usr);
        request.set_prop("full", "1");
        request.set_prop("item_type", (int)item_types::EMPTY_SCRIPT_BUNDLE);

        std::vector<mongo_requester> results = request.fetch_from_db(item_context);

        for(mongo_requester& req : results)
        {
            if(req.get_prop("registered_as") == "")
                continue;

            std::string item_id = req.get_prop("item_id");

            if(!loaded_user.has_loaded_item(item_id))
                continue;

            std::string name = usr + "." + req.get_prop("registered_as") + " `D[bundle]`";

            names.push_back(name);
        }
    }

    if(make_array)
    {
        push_duk_val(ctx, names);
    }
    else
    {
        std::string str = format_pretty_names(names);

        duk_push_string(ctx, str.c_str());
    }

    return 1;
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
        names.push_back("#" + req.get_prop("item_id"));
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

    {
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

        caller_usr.overwrite_user_in_db(mongo_user_info);
        destination_usr.overwrite_user_in_db(mongo_user_info);
    }

    {
        std::string cash_log = "`XCash xfer` | from: " + from  + ", to: " + to + ", amount: " + std::to_string(amount);

        int err = make_logs_on(ctx, from, user_node_info::GC_LOG, {cash_log});

        if(err)
            return err;

        err = make_logs_on(ctx, to, user_node_info::GC_LOG, {cash_log});

        if(err)
            return err;
    }

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

    int make_array = duk_get_prop_string_as_int(ctx, -1, "array");

    std::vector<std::string> names;

    for(auto& i : privileged_functions)
    {
        names.push_back("#" + i.first);
    }

    if(make_array)
    {
        push_duk_val(ctx, names);
    }
    else
    {
        std::string str = format_pretty_names(names);

        duk_push_string(ctx, str.c_str());
    }

    return 1;
}

size_t get_wall_time();
double get_wall_time_s();

inline
bool user_in_channel(mongo_lock_proxy& mongo_ctx, duk_context* ctx, const std::string& username, const std::string& channel)
{
    mongo_requester request;
    request.set_prop("channel_name", channel);

    auto found = request.fetch_from_db(mongo_ctx);

    if(found.size() != 1)
        return false;

    auto channel_users = str_to_array(found[0].get_prop("user_list"));

    return array_contains(channel_users, username);
}

inline
bool is_valid_channel_name(const std::string& in)
{
    for(auto& i : in)
    {
        if(!isalnum(i))
            return false;
    }

    return true;
}

inline
duk_ret_t msg__manage(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string to_join = duk_safe_get_prop_string(ctx, -1, "join");
    std::string to_leave = duk_safe_get_prop_string(ctx, -1, "leave");
    std::string to_create = duk_safe_get_prop_string(ctx, -1, "create");

    int num_set = 0;

    if(to_join.size() > 0)
    {
        if(!is_valid_channel_name(to_join))
            return push_error(ctx, "Invalid Name");

        num_set++;
    }

    if(to_leave.size() > 0)
    {
        if(!is_valid_channel_name(to_leave))
            return push_error(ctx, "Invalid Name");

        num_set++;
    }

    if(to_create.size() > 0)
    {
        if(!is_valid_channel_name(to_create))
            return push_error(ctx, "Invalid Name");

        num_set++;
    }

    if(num_set != 1)
        return push_error(ctx, "Only one leave/join/create parameter may be specified");

    if(to_join.size() >= 10 || to_leave.size() >= 10 || to_create.size() >= 10)
        return push_error(ctx, "Invalid Leave/Join/Create arguments");

    std::string username = get_caller(ctx);

    bool joining = to_join != "";

    if(to_join.size() > 0 || to_leave.size() > 0)
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(ctx));

        mongo_requester request;

        if(to_join != "")
            request.set_prop("channel_name", to_join);
        if(to_leave != "")
            request.set_prop("channel_name", to_leave);

        std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

        if(found.size() == 0)
            return push_error(ctx, "Channel does not exist");

        if(found.size() > 1)
            return push_error(ctx, "Some kind of catastrophic error: Yellow Sparrow");

        mongo_requester& chan = found[0];

        std::vector<std::string> users = str_to_array(chan.get_prop("user_list"));

        if(joining && array_contains(users, username))
            return push_error(ctx, "In channel");

        if(!joining && !array_contains(users, username))
            return push_error(ctx, "Not in Channel");

        if(joining)
        {
            users.push_back(username);
        }

        if(!joining)
        {
            auto it = std::find(users.begin(), users.end(), username);
            users.erase(it);
        }

        mongo_requester to_find = request;

        mongo_requester to_set;
        to_set.set_prop("user_list", array_to_str(users));

        to_find.update_in_db_if_exact(mongo_ctx, to_set);
    }

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        mongo_requester request;
        request.set_prop("name", get_caller(ctx));

        auto found = request.fetch_from_db(mongo_ctx);

        if(found.size() != 1)
            return push_error(ctx, "Catastrophic error: Red camel");

        mongo_requester& fuser = found[0];

        std::vector<std::string> chans = str_to_array(fuser.get_prop("joined_channels"));

        if(to_join != "" && !array_contains(chans, to_join))
        {
            chans.push_back(to_join);
        }

        if(to_leave != "" && array_contains(chans, to_leave))
        {
            auto it = std::find(chans.begin(), chans.end(), to_leave);

            chans.erase(it);
        }

        mongo_requester to_set;
        to_set.set_prop("joined_channels", array_to_str(chans));

        request.update_in_db_if_exact(mongo_ctx, to_set);
    }

    if(to_create.size() > 0)
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(ctx));

        mongo_requester request;
        request.set_prop("channel_name", to_create);

        if(request.fetch_from_db(mongo_ctx).size() > 0)
            return push_error(ctx, "Channel already exists");

        mongo_requester to_insert;
        to_insert.set_prop("channel_name", to_create);

        to_insert.insert_in_db(mongo_ctx);
    }

    return push_success(ctx);
}

inline
duk_ret_t msg__send(priv_context& priv_ctx, duk_context* ctx, int sl)
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

    std::vector<std::string> users;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(ctx));

        if(!user_in_channel(mongo_ctx, ctx, get_caller(ctx), channel))
            return push_error(ctx, "User not in channel or doesn't exist");

        mongo_requester request;
        request.set_prop("channel_name", channel);

        auto found = request.fetch_from_db(mongo_ctx);

        if(found.size() != 1)
            return push_error(ctx, "Something real weird happened: Orange Canary");

        mongo_requester& chan = found[0];

        users = str_to_array(chan.get_prop("user_list"));
    }

    bool found = false;

    for(auto& i : users)
    {
        if(i == get_caller(ctx))
        {
            found = true;
            break;
        }
    }

    if(!found)
        return push_error(ctx, "Not in channel");

    {
        ///TODO: LIMIT
        for(auto& current_user : users)
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(get_thread_id(ctx));
            mongo_ctx->change_collection(current_user);

            size_t real_time = get_wall_time();

            mongo_requester to_insert;
            to_insert.set_prop("user", get_caller(ctx));
            to_insert.set_prop("is_chat", 1);
            to_insert.set_prop("msg", msg);
            to_insert.set_prop("channel", channel);
            to_insert.set_prop_double("time_ms", real_time);
            //to_insert.set_prop("to_user", current_user);
            to_insert.set_prop("processed", 0);

            to_insert.insert_in_db(mongo_ctx);
        }
    }

    return push_success(ctx);
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

std::string prettify_chat_strings(std::vector<mongo_requester>& found);

inline
duk_ret_t msg__recent(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string channel = duk_safe_get_prop_string(ctx, -1, "channel");
    int num = duk_get_prop_string_as_int(ctx, -1, "count");
    bool pretty = !duk_get_prop_string_as_int(ctx, -1, "array");

    if(num <= 0)
        num = 10;

    if(channel.size() == 0)
        channel = "0000";

    std::cout << "fchannel " << channel << std::endl;

    if(!is_valid_channel_name(channel))
        return push_error(ctx, "Invalid channel name");

    if(channel == "" || num >= 100 || channel.size() >= 10)
    {
        push_error(ctx, "Usage: #ms.msg.recent({channel:\"<name>\", count:num, pretty:1})");
        return 1;
    }

    mongo_lock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(get_thread_id(ctx));
    mongo_ctx->change_collection(get_caller(ctx));

    ///ALARM: ALARM: RATE LIMIT

    mongo_requester request;
    request.set_prop("channel", channel);
    request.set_prop("is_chat", 1);
    request.set_prop_sort_on("time_ms", -1);

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
    }
    else
    {
        push_duk_val(ctx, names);
    }

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
std::string format_item(item& i, bool is_short, user& usr, user_nodes& nodes)
{
    if(is_short)
    {
        std::string str = i.get_prop("short_name");

        if(nodes.any_contains_lock(i.get_prop("item_id")))
        {
            str += " [on_node]";
        }

        return str;
    }

    std::string ret = "{\n";

    bool is_open_source = i.props.get_prop_as_integer("open_source");

    for(auto& p : i.props.properties)
    {
        if(!is_open_source && p.first == "unparsed_source")
            continue;

        if(!is_open_source && p.first == "parsed_source")
            continue;

        ret += "    " + p.first + ": " + p.second + ",\n";
    }

    if(usr.has_loaded_item(i.get_prop("item_id")))
        ret += "    loaded: true\n";

    if(nodes.any_contains_lock(i.get_prop("item_id")))
        ret += "    on_node: true";

    return ret + "}";
}

inline
duk_object_t get_item_raw(item& i, bool is_short, user& usr, user_nodes& nodes)
{
    duk_object_t obj;

    if(usr.has_loaded_item(i.get_prop("item_id")))
        obj["loaded"] = true;

    if(nodes.any_contains_lock(i.get_prop("item_id")))
        obj["on_node"] = true;

    if(is_short)
    {
        obj["short_name"] = i.get_prop("short_name");

        return obj;
    }

    bool is_open_source = i.get_prop_as_integer("open_source");

    for(auto& p : i.props.properties)
    {
        if(!is_open_source && p.first == "unparsed_source")
            continue;

        if(!is_open_source && p.first == "parsed_source")
            continue;

        if(is_short && p.first != "short_name")
            continue;

        obj[p.first] = p.second;
    }

    return obj;
}

/*inline
void change_item_raw(mongo_lock_proxy& mongo_ctx, int load_idx, int unload_idx, user& found_user)
{
    std::string tl = found_user.index_to_item(load_idx);
    std::string tul = found_user.index_to_item(unload_idx);

    ///NEED TO CHECK CONSTRAINTS HERE ALARM
    ///ALARM ALARM NEED TO PREVENT UNLOADABLE ITEMS FROM BEING LOADED!!!
    found_user.load_item(tl);
    found_user.unload_item(tul);

    found_user.overwrite_user_in_db(mongo_ctx);
}*/

inline
duk_ret_t load_item_raw(duk_context* ctx, int node_idx, int load_idx, int unload_idx, user& usr, user_nodes& nodes, std::string& accum)
{
    std::string to_load = usr.index_to_item(load_idx);
    std::string to_unload = usr.index_to_item(unload_idx);

    std::string which = to_load;

    if(to_unload.size() > 0)
        which = to_unload;

    if(which == "")
        return push_error(ctx, "Item not found");

    item next;

    {
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

        if(!next.exists_in_db(item_ctx, which))
            return push_error(ctx, "Something weird happened");

        next.load_from_db(item_ctx, which);
    }

    if(next.get_prop_as_integer("item_type") != (int)item_types::LOCK)
    {
        std::string tl = usr.index_to_item(load_idx);
        std::string tul = usr.index_to_item(unload_idx);

        ///NEED TO CHECK CONSTRAINTS HERE ALARM
        ///ALARM ALARM NEED TO PREVENT UNLOADABLE ITEMS FROM BEING LOADED!!!
        usr.load_item(tl);
        usr.unload_item(tul);

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

            usr.overwrite_user_in_db(mongo_ctx);
        }

        accum += "Performed non lock item operation";

        return 0;
    }

    if(which == to_load && node_idx == -1)
    {
        if(nodes.any_contains_lock(to_load))
            return push_error(ctx, "Already loaded");

        {
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));
            nodes.load_lock_to_any(item_ctx, to_load);
        }

        accum += "Loaded\n";
    }

    /*if(which == to_load && node_idx >= 0)
    {
        nodes.load_lock_to_id(to_load, node_idx);
    }*/

    if(which == to_unload && node_idx == -1)
    {
        nodes.unload_lock_from_any(to_unload);

        accum += "Unloaded\n";
    }

    //if(which == to_unload && node_idx )

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));
        nodes.overwrite_in_db(node_ctx);
    }

    return 0;
}

inline
void push_internal_items_view(duk_context* ctx, int pretty, int full, user_nodes& nodes, user& found_user)
{
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
                else if(nodes.any_contains_lock(item_id))
                    formatted += "`L" + std::to_string(idx) + "`: ";
                else
                    formatted += std::to_string(idx) + ": ";
            }

            formatted += format_item(next, !full, found_user, nodes);// + ",\n";

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

            objs.push_back(get_item_raw(next, !full, found_user, nodes));
        }

        push_duk_val(ctx, objs);
    }
}

inline
duk_ret_t items__manage(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int pretty = !duk_get_prop_string_as_int(ctx, -1, "array", 0);
    int full = duk_get_prop_string_as_int(ctx, -1, "full", 0);

    int load_idx = duk_get_prop_string_as_int(ctx, -1, "load", -1);
    int unload_idx = duk_get_prop_string_as_int(ctx, -1, "unload", -1);
    int node_idx = duk_get_prop_string_as_int(ctx, -1, "node", -1);

    if(load_idx >= 0 && unload_idx >= 0)
        return push_error(ctx, "Only one load/unload at a time");

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, get_caller(ctx));
        nodes.load_from_db(node_ctx, get_caller(ctx));
    }

    user found_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!found_user.load_from_db(mongo_ctx, get_caller(ctx)))
            return push_error(ctx, "No such user/really catastrophic error");
    }

    if(load_idx >= 0 || unload_idx >= 0)
    {
        std::string accum;

        auto ret = load_item_raw(ctx, node_idx, load_idx, unload_idx, found_user, nodes, accum);

        if(ret > 0)
            return ret;

        push_duk_val(ctx, accum);
        return 1;
    }

    push_internal_items_view(ctx, pretty, full, nodes, found_user);

    return 1;
}

inline
duk_ret_t push_xfer_item_with_logs(duk_context* ctx, int item_idx, const std::string& from, const std::string& to)
{
    item placeholder;

    if(placeholder.transfer_from_to_by_index(item_idx, from, to, get_thread_id(ctx)))
    {
        std::string xfer = "`NItem xfer` | from: " + from  + ", to: " + to + ", index: " + std::to_string(item_idx);

        make_logs_on(ctx, from, user_node_info::ITEM_LOG, {xfer});
        make_logs_on(ctx, to, user_node_info::ITEM_LOG, {xfer});

        duk_push_int(ctx, placeholder.get_prop_as_integer("item_id"));
    }
    else
        push_error(ctx, "Could not xfer");

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

    {
        user_nodes nodes;

        {
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

            nodes.ensure_exists(node_ctx, get_caller(ctx));
            nodes.load_from_db(node_ctx, get_caller(ctx));
        }

        user found_user;

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

            found_user.load_from_db(mongo_ctx, get_caller(ctx));

            if(!found_user.valid)
            {
                push_error(ctx, "No such user/really catastrophic error");
                return 1;
            }
        }

        std::string accum;

        auto ret = load_item_raw(ctx, -1, -1, item_idx, found_user, nodes, accum);

        if(ret > 0)
            return push_error(ctx, accum);
    }

    push_xfer_item_with_logs(ctx, item_idx, from, to);

    return 1;
}

inline
std::optional<std::pair<user, user_nodes>> get_user_and_nodes(duk_context* ctx, const std::string& name)
{
    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, get_caller(ctx));
        nodes.load_from_db(node_ctx, get_caller(ctx));
    }

    user found_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!found_user.load_from_db(mongo_ctx, get_caller(ctx)))
            return std::nullopt;
    }

    return std::pair(found_user, nodes);
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

    {
        mongo_lock_proxy user_lock = get_global_mongo_user_info_context(get_thread_id(ctx));

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

        /*item found_script;

        if(!found_bundle.exists_in_db(item_lock, full_script_name))
            return push_error(ctx, "No such script");

        found_script.load_from_db(item_lock, full_script_name);*/

        script_info found_script;
        found_script.name = full_script_name;

        if(!found_script.load_from_db(item_lock))
            return push_error(ctx, "No such script or invalid script");

        if(!found_script.valid)
            return push_error(ctx, "Script invalid");

        int max_storage = found_bundle.get_prop_as_integer("max_script_size");

        if((int)found_script.unparsed_source.size() > max_storage)
            return push_error(ctx, "Empty bundle does not contain enough space");

        found_script.fill_as_bundle_compatible_item(found_bundle);
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

    std::string lock_type = duk_safe_get_prop_string(ctx, -1, "lock_type");

    if(item_type == (int)item_types::LOCK)
    {
        bool found = false;

        for(auto& i : secret_map)
        {
            if(i.first == lock_type)
            {
                found = true;
                break;
            }
        }

        if(!found)
            return push_error(ctx, "No such lock");
    }

    test_item = item_types::get_default_of((item_types::item_type)item_type, lock_type);

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

inline
duk_ret_t items__expose(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int pretty = !duk_get_prop_string_as_int(ctx, -1, "array", 0);
    int full = duk_get_prop_string_as_int(ctx, -1, "full", 0);

    std::string from = duk_safe_get_prop_string(ctx, -1, "from");

    if(from == "")
        return push_error(ctx, "Args: from:<username>");

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, from);
        nodes.load_from_db(node_ctx, from);
    }

    user found_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!found_user.load_from_db(mongo_ctx, from))
            return push_error(ctx, "No such user/really catastrophic error");
    }

    auto hostile = nodes.valid_hostile_actions();

    if((hostile & user_node_info::XFER_ITEM_FROM) > 0)
    {
        push_internal_items_view(ctx, pretty, full, nodes, found_user);

        return 1;
    }
    else
    {
        return push_error(ctx, "System Breach Node Secured");
    }

    return 1;
}

///have items__steal reset internal node structure
inline
duk_ret_t items__steal(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    std::string from = duk_safe_get_prop_string(ctx, -1, "from");

    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);

    if(from == "" || item_idx < 0)
        return push_error(ctx, "Args: from:<username>, idx:item_offset");

    auto found = get_user_and_nodes(ctx, from);

    if(!found.has_value())
        return push_error(ctx, "Error or no such user");

    user found_user = found->first;
    user_nodes nodes = found->second;

    std::string accum;

    ///unloads item if loaded
    auto ret = load_item_raw(ctx, -1, -1, item_idx, found_user, nodes, accum);

    if(ret > 0)
        return push_error(ctx, accum);

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.reset_all_breach();

        nodes.overwrite_in_db(node_ctx);
    }

    push_xfer_item_with_logs(ctx, item_idx, from, get_caller(ctx));
    return 1;
}

inline
duk_ret_t cash__steal(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string from = duk_safe_get_prop_string(ctx, -1, "from");

    if(from == "")
        return push_error(ctx, "Args: from:<username>, amount:<number>");

    double amount = duk_safe_get_generic(duk_get_number, ctx, -1, "amount", 0);

    if(amount == 0)
        return push_error(ctx, "amount is not a number, or 0");

    user target;

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!target.load_from_db(user_ctx, from))
            return push_error(ctx, "Target does not exist");
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, from);
        nodes.load_from_db(node_ctx, from);
    }

    auto hostile = nodes.valid_hostile_actions();

    if((hostile & user_node_info::XFER_GC_FROM) > 0)
    {
        return cash_internal_xfer(ctx, from, get_caller(ctx), amount);
    }
    else
    {
        return push_error(ctx, "System Breach Node Secured");
    }
}


///bear in mind that this function is kind of weird
///ok so: going for standard lock stack initially, and will use standard breached state
///will initially have two easy locks
///after this, swap to node based system, where the user port takes to you
///a frontal node, which then reveals a netted internal structure
///initially lets have: GC transactions, item transactions, and a breach node
///which would breach a system in the traditional sense
///once a node has been breached, it will enter a breached state
///aka if the breach node is breached, you can do cool stuff with it (aka i don't have them implemented as nodes yet)

///Maybe initially I should actually create a breach node as that initial entry node, check its breach state, and then
///use that to determine breach status, so I don't end up hardcoding down lock stack too much
///will need a separate db for nodes?

///alright, i'm going to pump for sooner vs later
///so: We need a node based api
///we need a node to have an id, and a type
///nodes need to store which user they belong to, probably as part of their name (eg node_i20k_32) or (node i20k 32)

#if 0
inline
duk_ret_t user__port(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = get_host_from_fullname(priv_ctx.called_as);

    //std::cout << "user_name " << name_of_person_being_attacked << std::endl;

    user usr;

    {
        mongo_lock_proxy user_info = get_global_mongo_user_info_context(get_thread_id(ctx));
        user_info->change_collection(name_of_person_being_attacked);

        usr.load_from_db(user_info, name_of_person_being_attacked);
    }

    std::vector<std::string> loaded_items = usr.all_loaded_items();

    //std::cout << "lsize " << loaded_items.size() << std::endl;

    std::vector<item> all_loaded_attackables;

    std::map<std::string, int> seen_attackables;

    {
        mongo_lock_proxy item_info = get_global_mongo_user_items_context(get_thread_id(ctx));

        for(auto& id : loaded_items)
        {
            item next;
            next.load_from_db(item_info, id);

            //std::cout << "fid " << id << std::endl;

            //std::cout << "ftype " << next.get_prop("item_type") << std::endl;

            if(seen_attackables[next.get_prop("lock_type")])
                continue;

            if(next.get_prop_as_integer("item_type") != item_types::LOCK)
                continue;

            all_loaded_attackables.push_back(next);

            seen_attackables[next.get_prop("lock_type")] = 1;
        }
    }

    bool all_success = true;

    std::string msg;

    for(item& i : all_loaded_attackables)
    {
        std::string func = i.get_prop("lock_type");

        auto it = secret_map.find(func);

        if(it != secret_map.end())
        {
            if(!it->second(priv_ctx, ctx, msg))
            {
                all_success = false;

                break;
            }
        }
    }

    finalise_info(msg, all_success);

    if(msg.size() > 0 && msg.back() == '\n')
        msg.pop_back();

    duk_push_string(ctx, msg.c_str());
    return 1;
}
#endif // 0


inline
duk_ret_t nodes__view_log(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = duk_safe_get_prop_string(ctx, -1, "name");

    int make_array = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "array", 0);

    user usr;

    {
        mongo_lock_proxy user_info = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!usr.load_from_db(user_info, name_of_person_being_attacked))
            return push_error(ctx, "No such user");
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, name_of_person_being_attacked);
        nodes.load_from_db(node_ctx, name_of_person_being_attacked);
    }

    std::string node_fullname = duk_safe_get_prop_string(ctx, -1, "NID");

    std::vector<item> attackables;

    user_node* current_node = nullptr;

    {
        current_node = nodes.name_to_node(name_of_person_being_attacked + "_" + node_fullname);

        {
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

            attackables = current_node->get_locks(item_ctx);
        }
    }

    if(current_node == nullptr)
        return push_error(ctx, "Misc error: Blue Melon");

    if(name_of_person_being_attacked != get_caller(ctx))
    {
        ///there must be both an accessible path, and the node itself must be breached
        if(!nodes.node_accessible(*current_node) || !current_node->is_breached())
        {
            duk_push_string(ctx, nodes.get_lockdown_message().c_str());
            return 1;
        }
    }

    std::vector<std::string> logs = current_node->logs;

    if(make_array)
    {
        push_duk_val(ctx, current_node->logs);
        return 1;
    }
    else
    {
        std::string str = format_pretty_names(logs);

        push_duk_val(ctx, str);
        return 1;
    }

    return 1;
}

inline
duk_ret_t user__port(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = get_host_from_fullname(priv_ctx.called_as);

    //std::cout << "user_name " << name_of_person_being_attacked << std::endl;

    user usr;

    {
        mongo_lock_proxy user_info = get_global_mongo_user_info_context(get_thread_id(ctx));

        usr.load_from_db(user_info, name_of_person_being_attacked);
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, name_of_person_being_attacked);
        nodes.load_from_db(node_ctx, name_of_person_being_attacked);
    }

    std::string node_fullname = duk_safe_get_prop_string(ctx, -1, "NID");

    std::vector<item> attackables;

    user_node* current_node = nullptr;

    //if(node_fullname == "")
    {
        //current_node = nodes.get_front_node();

        current_node = nodes.name_to_node(name_of_person_being_attacked + "_" + node_fullname);

        {
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

            attackables = current_node->get_locks(item_ctx);
        }
    }

    if(current_node == nullptr)
        return push_error(ctx, "Misc error: Black Tiger");

    if(!nodes.node_accessible(*current_node))
    {
        duk_push_string(ctx, nodes.get_lockdown_message().c_str());
        return 1;

        //return push_error(ctx, nodes.get_lockdown_message());
    }

    ///leave trace in logs
    {
        user attacker;

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

            attacker.load_from_db(mongo_ctx, get_caller(ctx));
        }

        std::string port = attacker.user_port;

        nodes.leave_trace(*current_node, attacker.name, port);

        ///hmm, we are actually double overwriting here
        {
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

            nodes.overwrite_in_db(node_ctx);
        }
    }

    ///if(current_node.breached)
    ///do display adjacents, node type, what we can do here

    bool all_success = true;

    std::string msg;

    if(!current_node->is_breached())
    {
        for(item& i : attackables)
        {
            if(i.should_rotate())
            {
                i.handle_rotate();

                mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));
                i.overwrite_in_db(item_ctx);
            }

            std::string func = i.get_prop("lock_type");

            auto it = secret_map.find(func);

            if(it != secret_map.end())
            {
                if(!it->second(priv_ctx, ctx, msg, i))
                {
                    all_success = false;

                    break;
                }
            }
        }
    }

    if(current_node->is_breached())
    {
        msg += current_node->get_breach_message(nodes);
    }

    ///do info here first, then display the breach message the next time round
    finalise_info(msg, all_success, current_node->is_breached(), attackables.size());

    if(all_success && !current_node->is_breached())
    {
        msg += current_node->get_breach_message(nodes);
    }

    if(all_success)
    {
        current_node->breach();

        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.overwrite_in_db(node_ctx);
    }

    duk_push_string(ctx, msg.c_str());

    return 1;
}


inline
duk_ret_t nodes__manage(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        ///yeah this isn't good enough, need to do what we did for locs?
        ///or just do it in loc handler i guess
        nodes.ensure_exists(node_ctx, get_caller(ctx));

        nodes.load_from_db(node_ctx, get_caller(ctx));
    }

    user usr;

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        usr.load_from_db(user_ctx, get_caller(ctx));
    }


    std::string accum = "Node Key: ";

    for(int i=0; i < (int)user_node_info::TYPE_COUNT; i++)
    {
        accum += user_node_info::short_name[i] + ": " + user_node_info::long_names[i];

        if(i != user_node_info::TYPE_COUNT-1)
            accum += ", ";
    }

    accum += "\n";

    {
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

        ///this needs to take a user as well
        ///so that we can display the indices of the items for easy load/unload
        for(user_node& node : nodes.nodes)
        {
            accum += node.get_pretty(item_ctx, usr);
        }
    }

    duk_push_string(ctx, accum.c_str());
    return 1;
}

inline
duk_ret_t nodes__port(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    user usr;

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        usr.load_from_db(user_ctx, get_caller(ctx));
    }

    std::string ret = usr.name + "." + usr.user_port;

    duk_push_string(ctx, ret.c_str());
    return 1;
}

///need to strictly define the conditions which allow you to
///view the status of this item in the network and view its links
///requirements:
///must be extremely straightforward to do. One step process on unguarded nodes
///so. If the node we're using this for is not locked, we freely view connections... without breaching perhaps?
///otherwise must be breached
///not sure on literally autobreaching but might be more consistent
///Ok. So, you must pwn the front node to expose
///what about to hack behind
///Pwn breach node again? Use the permissions system and fill in later?
///node system can happily placeholder still
///so. When we try and hack something then, will likely have to be
///#sys.hack({target:"name", NID:1212, hello:true}), kind of clunky
///maybe i should start introducing short sytax
///!name({arg:whatever}) = #sys.hack({name:"name", arg:whatever})
///and ?name() = #net.view({target:"name"})
///integrate into parser so you can script? maybe #!name
///#sys.hack will have to take a path
///so #sys.hack({path:["name1", "name2", "name3"]}), which will have to be abbreviated to
///in the event that path isn't a full path, a* to the first element, and from the last element
///allows easy routing
///#!name1.name2.name3, or #!name3 will take the shortest path by a*

///ok. viewing and autobreaching should leave no logs, viewing and manually breaching will leave logs as per usual (ie none i believe), but
///it carries that same degree of hostility
///accessing a node behind a current node should require a higher degree of breaching

///this function is inadequate
///we need a proper 2d representation
///we need a n step map, and additionally

///this function needs to respect locks and breaching etc
inline
duk_ret_t net__view(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string from = duk_safe_get_prop_string(ctx, -1, "from");
    int pretty = !duk_get_prop_string_as_int(ctx, -1, "array", 0);

    if(from == "")
        return push_error(ctx, "usage: net.view({from:<username>})");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    auto links = playspace_network_manage.get_links(from);

    if(!pretty)
    {
        push_duk_val(ctx, links);
    }
    else
    {
        std::string str = format_pretty_names(links);

        push_duk_val(ctx, str);
    }

    return 1;
}

inline
duk_ret_t net__map(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int w = 40;
    int h = 30;

    std::vector<std::string> str;

    //for(int i=0; i < w * h; i++)
    //    str.push_back(' ');

    for(int y=0; y < h; y++)
    {
        for(int x=0; x < w-1; x++)
        {
            str.push_back(" ");
        }

        str.push_back("\n");
    }

    std::string from = duk_safe_get_prop_string(ctx, -1, "from");
    int num = duk_get_prop_string_as_int(ctx, -1, "n", 2);

    if(from == "")
        return push_error(ctx, "usage: net.map({from:<username>, num:2})");

    if(num < 0 || num >= 10)
        return push_error(ctx, "num out of range [1,10]");

    vec2i centre = {w/2, h/2};

    int spacing = 3;

    //std::vector<std::set<std::string>> rings;
    //rings.resize(num);

    std::map<std::string, int> rings;

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();
    //auto links = playspace_network_manage.get_links(from)
    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::vector<std::string> next_ring;
    std::vector<std::string> current_ring{from};

    std::map<std::string, std::string> display_string;
    int overall_count = 0;

    for(int ring = 0; ring < num; ring++)
    {
        next_ring.clear();

        for(auto& i : current_ring)
        {
            display_string[i] = chars[overall_count];
            overall_count++;
            overall_count %= chars.size();
        }

        for(const std::string& str : current_ring)
        {
            if(rings.find(str) != rings.end())
                continue;

            rings[str] = ring;

            auto connections = playspace_network_manage.get_links(str);

            for(auto& i : connections)
            {
                if(rings.find(i) != rings.end())
                    continue;

                next_ring.push_back(i);
            }
        }

        current_ring = next_ring;
    }

    std::map<int, std::vector<std::string>> ring_to_nodes;

    for(auto& i : rings)
    {
        ring_to_nodes[i.second].push_back(i.first);
    }

    std::map<std::string, vec2i> node_to_pos;

    for(int ring = 0; ring < num; ring++)
    {
        auto nodes = ring_to_nodes[ring];

        for(int i=0; i < (int)nodes.size(); i++)
        {
            float frac = (float)i / nodes.size();

            float angle = frac * 2 * M_PI;

            vec2f offset = {spacing*ring, 0.f};
            offset = offset.rot(angle);

            vec2i next = centre + (vec2i){offset.x(), offset.y()};

            next = clamp(next, (vec2i){0, 0}, (vec2i){w, h}-1);

            node_to_pos[nodes[i]] = next;

            //str[next.y() * w + next.x()] = chars[overall_count];
            //overall_count++;
        }
    }

    for(auto& i : node_to_pos)
    {
        const std::string& name = i.first;
        vec2i pos = i.second;

        auto connections = playspace_network_manage.get_links(name);

        int colour_offset_count = 0;

        for(auto& conn : connections)
        {
            auto found = node_to_pos.find(conn);

            if(found == node_to_pos.end())
                continue;

            vec2i to_draw_pos = found->second;

            vec2f out_dir;
            int out_num;

            line_draw_helper((vec2f){pos.x(), pos.y()}, (vec2f){to_draw_pos.x(), to_draw_pos.y()}, out_dir, out_num);

            /*vec2i idiff = to_draw_pos - pos;

            vec2f fdiff = (vec2f){idiff.x(), idiff.y()};

            out_dir = fdiff.norm();
            out_num = fdiff.length();*/

            vec2f cur = (vec2f){pos.x(), pos.y()};

            for(int i=0; i < out_num; i++)
            {
                vec2f rpos = round(cur);
                vec2i ipos = clamp((vec2i){rpos.x(), rpos.y()}, (vec2i){0,0}, (vec2i){w-1, h-1});

                std::string col = string_to_colour(name);

                if((colour_offset_count % 2) == 1)
                    col = string_to_colour(conn);

                str[ipos.y() * w + ipos.x()] = "`" + col + ".`";

                cur += out_dir;
            }

            colour_offset_count++;
        }
    }

    for(auto& i : node_to_pos)
    {
        vec2i clamped = clamp(i.second, (vec2i){0, 0}, (vec2i){w-1, h});

        std::string to_display = "`" + string_to_colour(i.first) + display_string[i.first] + "`";

        str[clamped.y() * w + clamped.x()] = to_display;

        //str[clamped.y() * w + clamped.x()] = display_string[i.first].front();
    }

    std::string built;

    for(auto& i : str)
    {
        built += i;
    }

    push_duk_val(ctx, built);

    //push_duk_val(ctx, str);

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
#define REGISTER_FUNCTION_PRIV_3(x, y, z) {parse_function_hack(#x), {&x, y, z}}

inline
std::map<std::string, priv_func_info> privileged_functions
{
    REGISTER_FUNCTION_PRIV(cash__balance, 3),
    REGISTER_FUNCTION_PRIV(cash__xfer_to, 2),
    REGISTER_FUNCTION_PRIV(cash__xfer_to_caller, 4),
    REGISTER_FUNCTION_PRIV(cash__steal, 4),
    REGISTER_FUNCTION_PRIV(scripts__get_level, 4),
    REGISTER_FUNCTION_PRIV(scripts__core, 4),
    REGISTER_FUNCTION_PRIV(scripts__me, 2),
    REGISTER_FUNCTION_PRIV(scripts__public, 4),
    REGISTER_FUNCTION_PRIV(msg__manage, 3),
    REGISTER_FUNCTION_PRIV(msg__send, 3),
    REGISTER_FUNCTION_PRIV(msg__recent, 2),
    REGISTER_FUNCTION_PRIV(users__me, 0),
    REGISTER_FUNCTION_PRIV(items__create, 0),
    REGISTER_FUNCTION_PRIV(items__steal, 4),
    REGISTER_FUNCTION_PRIV(items__expose, 4),
    //REGISTER_FUNCTION_PRIV(sys__disown_upg, 0),
    //REGISTER_FUNCTION_PRIV(sys__xfer_upgrade_uid, 0),
    REGISTER_FUNCTION_PRIV(items__xfer_to, 1),
    REGISTER_FUNCTION_PRIV(items__manage, 2),
    REGISTER_FUNCTION_PRIV(items__bundle_script, 1),
    REGISTER_FUNCTION_PRIV(items__register_bundle, 0),
    //REGISTER_FUNCTION_PRIV(user__port, 0), ///should this exist? It has to currently for dumb reasons ///nope, it needs special setup
    REGISTER_FUNCTION_PRIV(nodes__manage, 1),
    REGISTER_FUNCTION_PRIV(nodes__port, 1),
    REGISTER_FUNCTION_PRIV(nodes__view_log, 1),
    REGISTER_FUNCTION_PRIV(net__view, 4),
    REGISTER_FUNCTION_PRIV(net__map, 4),
};

std::map<std::string, std::vector<script_arg>> construct_core_args();

extern
std::map<std::string, std::vector<script_arg>> privileged_args;

inline
priv_func_info user_port_descriptor = {&user__port, 0};

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
