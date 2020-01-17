#include "privileged_core_scripts.hpp"

#include <set>
#include <deque>

#include "scheduled_tasks.hpp"
#include "command_handler.hpp"
#include "duk_object_functions.hpp"
#include "ascii_helpers.hpp"
#include <secret/low_level_structure.hpp>
#include <secret/npc_manager.hpp>
#include "rate_limiting.hpp"
#include "memory_sandbox.hpp"
#include "auth.hpp"
#include <secret/secret.hpp>
#include "perfmon.hpp"
#include "time.hpp"
#include "quest_manager.hpp"
#include "rng.hpp"
#include "argument_object.hpp"

#define SECLEVEL_FUNCTIONS

#define XFER_PATHS

std::map<std::string, std::vector<script_arg>> privileged_args = construct_core_args();
std::map<std::string, script_metadata> privileged_metadata = construct_core_metadata();

std::vector<script_arg> make_cary()
{
    return std::vector<script_arg>();
}

template<typename V, typename W, typename... T>
std::vector<script_arg> make_cary(V&& v, W&& w, T&&... t)
{
    std::vector<script_arg> args{{v, w}};

    auto next = make_cary(t...);

    args.insert(args.end(), next.begin(), next.end());

    return args;
}

template<typename... T>
std::vector<script_arg> make_carg(T&&... t)
{
    return make_cary(t...);
}

std::vector<arg_metadata> make_met()
{
    return std::vector<arg_metadata>();
}

template<typename T, typename U, typename V, typename... W>
std::vector<arg_metadata> make_met(T&& t, U&& u, V&& v, W&&... w)
{
    arg_metadata arg;
    arg.key_name = t;
    arg.val_text = u;
    arg.type = (arg_metadata::arg_type)v;

    std::vector<arg_metadata> ret{arg};

    auto next = make_met(w...);

    ret.insert(ret.end(), next.begin(), next.end());

    return ret;
}

template<typename... T>
std::vector<arg_metadata> make_met(const arg_metadata& arg, T&&... t)
{
    std::vector<arg_metadata> ret{arg};

    auto next = make_met(t...);

    ret.insert(ret.end(), next.begin(), next.end());

    return ret;
}

///make a new one that also takes a whole arg

std::map<std::string, std::vector<script_arg>> construct_core_args()
{
    std::map<std::string, std::vector<script_arg>> ret;

    ret["cash.balance"] = make_cary();
    ret["cash.expose"] = make_cary("user", "\"\"");
    ret["scripts.get_level"] = make_cary("name", "\"\"");
    ret["scripts.me"] = make_cary();
    ret["scripts.public"] = make_cary();
    ret["scripts.info"] = make_cary("name", "\"\"");
    ret["cash.xfer_to"] = make_cary("user", "\"\"", "amount", "0");
    ret["cash.xfer_to_caller"] = make_cary();
    ret["scripts.core"] = make_cary();

    ret["channel.create"] = make_cary("name", "\"\"", "password", "\"\"");
    ret["channel.join"] = make_cary("name", "\"\"");
    ret["channel.leave"] = make_cary("name", "\"\"");
    ret["channel.list"] = make_cary();

    ret["msg.manage"] = make_cary();
    ret["msg.send"] = make_cary("channel", "\"\"", "msg", "\"\"");
    ret["msg.tell"] = make_cary("user", "\"\"", "msg", "\"\"");
    ret["msg.recent"] = make_cary("channel", "\"\"", "count", "99");
    ret["users.me"] = make_cary();
    ret["users.accessible"] = make_cary();
    ret["item.steal"] = make_cary("user", "\"\"", "idx", "0");
    ret["item.expose"] = make_cary("user", "\"\"");
    ret["item.manage"] = make_cary();
    ret["item.list"] = make_cary();
    ret["item.load"] = make_cary("idx", "0");
    ret["item.unload"] = make_cary("idx", "0");
    ret["item.cull"] = make_cary("idx", "0");
    ret["item.xfer_to"] = make_cary("idx", "0", "user", "\"\"");
    ret["item.bundle_script"] = make_cary("idx", "0", "name", "\"\"", "tag", "\"\"");
    ret["item.register_bundle"] = make_cary("idx", "0", "name", "\"\"");
    ret["item.configure_on_breach"] = make_cary("idx", "0", "name", "\"\"");
    ret["cash.steal"] = make_cary("user", "\"\"", "amount", "0");
    //ret["user.port"] = make_cary();
    ret["nodes.manage"] = make_cary();
    ret["nodes.port"] = make_cary();
    ret["nodes.view_log"] = make_cary("user", "\"\"", "NID", "-1");
    ret["log.expose"] = make_cary("user", "\"\"", "NID", "-1");
    ret["net.view"] = make_cary("user", "\"\"", "n", "-1");
    ret["net.map"] = make_cary("user", "\"\"", "n", "6");
    //ret["net.links"] = make_cary("user", "\"\"", "n", "6");
    ret["net.hack"] = make_cary("user", "\"\"", "NID", "-1");
    //ret["net.access"] = make_cary("user", "\"\"");
    ret["net.switch"] = make_cary("user", "\"\"");
    //ret["net.modify"] = make_cary("user", "\"\"", "target", "\"\"");
    //ret["net.move"] = make_cary("user", "\"\"", "target", "\"\"");
    ret["net.path"] = make_cary("user", "\"\"", "target", "\"\"", "min_stability", "0");
    ret["sys.view"] = make_cary("user", "\"\"", "n", "-1", "fit", "false");
    ret["sys.map"] = make_cary("n", "-1", "centre", "false");
    ret["sys.move"] = make_cary("to", "\"\"", "queue", "false");
    ret["sys.access"] = make_cary("user", "\"\"");
    ret["sys.limits"] = make_cary();
    ret["ada.access"] = make_cary();
    ret["able.help"] = make_cary();

    return ret;
}

std::map<std::string, script_metadata> construct_core_metadata()
{
    std::map<std::string, script_metadata> ret;

    arg_metadata ok_arg;
    ok_arg.key_name = "ok";
    ok_arg.val_text = "error code";
    ok_arg.type = arg_metadata::OK;

    arg_metadata array_arg;
    array_arg.key_name = "array";
    array_arg.val_text = "Asks for scriptable output";
    array_arg.type = (arg_metadata::arg_type)(arg_metadata::SCRIPTABLE | arg_metadata::BOOLEAN | arg_metadata::OPT);

    ret["cash.balance"].description = "Gives your current cash balance";
    ret["cash.balance"].return_data = make_met("", "Cash Balance", arg_metadata::CASH | arg_metadata::NUMERIC);

    ret["cash.expose"].description = "Displays a hostile targets cash balance";
    ret["cash.expose"].return_data = make_met("available", "Cash Balance that can be stolen currently", arg_metadata::CASH | arg_metadata::NUMERIC, "exposed", "Total Cash Balance", arg_metadata::CASH | arg_metadata::NUMERIC, ok_arg);
    ret["cash.expose"].param_data = make_met("user", "User to Expose", arg_metadata::USER, array_arg);
    ret["cash.expose"].requires_breach = true;

    ret["cash.xfer_to"].description = "Sends cash to someone else";
    ret["cash.xfer_to"].return_data = make_met(ok_arg);
    ret["cash.xfer_to"].param_data = make_met("user", "to xfer to", arg_metadata::USER, "amount", "Amount to xfer", arg_metadata::CASH | arg_metadata::NUMERIC);

    ret["cash.xfer_to_caller"].description = "Xfers cash from the person hosting the script to the person calling the script, aka in reverse";
    ret["cash.xfer_to_caller"].return_data = make_met(ok_arg);
    ret["cash.xfer_to_caller"].param_data = make_met("amount", "Amount to xfer", arg_metadata::CASH | arg_metadata::NUMERIC);

    ret["cash.steal"].description = "Xfers cash from a hostile user";
    ret["cash.steal"].return_data = make_met(ok_arg);
    ret["cash.steal"].param_data = make_met("user", "User to steal from", arg_metadata::USER, "amount", "Amount to xfer", arg_metadata::CASH | arg_metadata::NUMERIC);
    ret["cash.steal"].requires_breach = true;

    ret["scripts.get_level"].description = "Gets the security level of a script";
    ret["scripts.get_level"].return_data = make_met("", "Security Level", arg_metadata::SECURITY_LEVEL, ok_arg);
    ret["scripts.get_level"].param_data = make_met("name", "script name", arg_metadata::SCRIPT);

    ret["scripts.core"].description = "List of Core game scripts";
    ret["scripts.core"].return_data = make_met("", "List of Scripts", arg_metadata::ARRAY);

    ret["scripts.me"].description = "Gets a list of your own scripts";
    ret["scripts.me"].return_data = make_met("", "Stringified result", arg_metadata::STRING, "", "Array Result", arg_metadata::ARRAY);
    ret["scripts.me"].param_data = make_met(array_arg);

    ret["scripts.public"].description = "Gets a list of public scripts";
    ret["scripts.public"].return_data = make_met("", "Stringified result", arg_metadata::STRING, "", "Array Result", arg_metadata::ARRAY);
    ret["scripts.public"].param_data = make_met("seclevel", "Filter By Seclevel", arg_metadata::SECURITY_LEVEL, array_arg);

    ret["scripts.info"].description = "Gets help information about a script";
    ret["scripts.info"].return_data = make_met("", "Stringified result", arg_metadata::STRING, "", "Script info, contains .keys, and .desc", arg_metadata::OBJECT);
    ret["scripts.info"].param_data = make_met("name", "Script name", arg_metadata::SCRIPT, array_arg);

    ret["channel.create"].description = "Create a Channel";
    ret["channel.create"].return_data = make_met(ok_arg);
    ret["channel.create"].param_data = make_met("name", "Channel to Create", arg_metadata::CHANNEL, "password", "Channel Password", arg_metadata::STRING);

    ret["channel.join"].description = "Join a Channel";
    ret["channel.join"].return_data = make_met(ok_arg);
    ret["channel.join"].param_data = make_met("name", "Channel to Join", arg_metadata::CHANNEL, "password", "Channel Password", arg_metadata::STRING);

    ret["channel.leave"].description = "Leave a Channel";
    ret["channel.leave"].return_data = make_met(ok_arg);
    ret["channel.leave"].param_data = make_met("name", "Channel to Leave", arg_metadata::CHANNEL);

    ret["channel.list"].description = "Get a list of joined Channels";
    ret["channel.list"].return_data = make_met("", "Stringified list of Channels", arg_metadata::STRING, "", "Array of Channels", arg_metadata::CHANNEL | arg_metadata::ARRAY);
    ret["channel.list"].param_data = make_met();

    ret["msg.manage"].description = "Join, Leave, or Create a channel";
    ret["msg.manage"].return_data = make_met(ok_arg);
    ret["msg.manage"].param_data = make_met("join", "Channel to Join", arg_metadata::CHANNEL, "leave", "Channel to Leave", arg_metadata::CHANNEL, "create", "Channel to Create", arg_metadata::CHANNEL);

    ret["msg.send"].description = "Send a message in a channel, or in the local system";
    ret["msg.send"].return_data = make_met(ok_arg);
    ret["msg.send"].param_data = make_met("channel", "Channel to send in", arg_metadata::CHANNEL, "msg", "Message", arg_metadata::STRING);

    ret["msg.tell"].description = "Send a whisper to a specific person";
    ret["msg.tell"].return_data = make_met(ok_arg);
    ret["msg.tell"].param_data = make_met("user", "User to Whisper", arg_metadata::USER, "msg", "Message", arg_metadata::STRING);

    ret["msg.recent"].description = "Get a list of recent messages in a channel";
    ret["msg.recent"].return_data = make_met("", "Stringified list of messages", arg_metadata::STRING, "", "Array of objects containing messages", arg_metadata::ARRAY, ok_arg);
    ret["msg.recent"].param_data = make_met("channel", "Channel to Retrieve", arg_metadata::CHANNEL, "count", "Number of Messages to fetch", arg_metadata::INTEGER,
                                            "tell", "Set to retrieve tells instead", arg_metadata::INTEGER | arg_metadata::OPT, array_arg);

    ret["users.me"].description = "Get a list of your own users, excluding npcs";
    ret["users.me"].return_data = make_met("", "Stringified list of users", arg_metadata::STRING, "", "Array of Users", arg_metadata::ARRAY, ok_arg);
    ret["users.me"].param_data = make_met(array_arg);

    ret["users.accessible"].description = "Get a list of npcs you have access to";
    ret["users.accessible"].return_data = make_met("", "Stringified list of users", arg_metadata::STRING, "", "Array of Users", arg_metadata::ARRAY, ok_arg);
    ret["users.accessible"].param_data = make_met(array_arg);

    ret["item.steal"].description = "Steals an item from a user";
    ret["item.steal"].return_data = make_met("", "Array of return messages", arg_metadata::ARRAY, ok_arg);
    ret["item.steal"].param_data = make_met("user", "User to Steal from", arg_metadata::USER, "idx", "Item Index/Indices", arg_metadata::ITEM_IDX | arg_metadata::ARRAY);
    ret["item.steal"].requires_breach = true;

    ret["item.expose"].description = "Shows a list of items on a user";
    ret["item.expose"].return_data = make_met("", "Stringified items", arg_metadata::STRING, "available", "How many items can be stolen currently", arg_metadata::INTEGER, "exposed", "Array of Items", arg_metadata::ARRAY, ok_arg);
    ret["item.expose"].param_data = make_met("user", "User to Expose Items on", arg_metadata::USER, "full", "Should expose item properties", arg_metadata::BOOLEAN | arg_metadata::OPT, array_arg);
    ret["item.expose"].requires_breach = true;

    ret["item.xfer_to"].description = "Transfers an item to another player";
    ret["item.xfer_to"].return_data = make_met(ok_arg);
    ret["item.xfer_to"].param_data = make_met("user", "User to Xfer to", arg_metadata::USER, "idx", "Item Index to Xfer", arg_metadata::ITEM_IDX);

    ret["item.manage"].description = "Load, unload, or swap items. With no arguments, views items";
    ret["item.manage"].return_data = make_met("", "Stringified list of items", arg_metadata::STRING, "", "Stringified Operation Result", arg_metadata::STRING, ok_arg);
    ret["item.manage"].param_data = make_met("load", "Item Idx to load", arg_metadata::ITEM_IDX, "unload", "Item Idx to unload", arg_metadata::ITEM_IDX, "full", "Displays all item properties", arg_metadata::BOOLEAN, "node", "Loads to a specific node", arg_metadata::NODE_IDX | arg_metadata::NODE_STRING, array_arg);

    ret["item.list"].description = "Returns a list of items";
    ret["item.list"].return_data = make_met("", "Stringified list of items", arg_metadata::STRING, ok_arg);
    ret["item.list"].param_data = make_met("full", "Displays all item properties", arg_metadata::BOOLEAN | arg_metadata::OPT, array_arg);

    ret["item.load"].description = "Loads an item or lock";
    ret["item.load"].return_data = make_met("", "Status Message", arg_metadata::STRING, ok_arg);
    ret["item.load"].param_data = make_met("idx", "Item Idx to load", arg_metadata::ITEM_IDX, "node", "Loads to a specific node. Only for locks", arg_metadata::NODE_IDX | arg_metadata::NODE_STRING | arg_metadata::OPT);

    ret["item.unload"].description = "Unloads an item or lock";
    ret["item.unload"].return_data = make_met("", "Status Message", arg_metadata::STRING, ok_arg);
    ret["item.unload"].param_data = make_met("idx", "Item Idx to unload", arg_metadata::ITEM_IDX);

    ///bundle script
    ///register bundle
    ///configure on breach
    ///nodes manage
    ///nodes view log

    ///deprecated net view and map

    ret["log.expose"].description = "Exposes the logs on a node";
    ret["log.expose"].return_data = make_met("", "Node Logs", arg_metadata::STRING, array_arg);
    ret["log.expose"].param_data = make_met("user", "User to Expose", arg_metadata::USER, "NID", "Node to Expose Logs On", arg_metadata::NODE_IDX);

    ret["net.hack"].description = "Hack another user or npc";
    ret["net.hack"].return_data = make_met("", "Hacking Output", arg_metadata::STRING, ok_arg);
    ret["net.hack"].param_data = make_met("user", "User to Hack", arg_metadata::USER);

    ///net.switch
    ///net.path

    ret["sys.map"].description = "Shows a map of all systems, from your perspective";
    ret["sys.map"].return_data = make_met("", "Ascii Map of Systems", arg_metadata::STRING, "", "Array of System Objects", arg_metadata::ARRAY);
    ret["sys.map"].param_data = make_met("centre", "Should Centre Map", arg_metadata::BOOLEAN | arg_metadata::OPT,
                                         "n", "Number of systems depthwise to display from location", arg_metadata::INTEGER | arg_metadata::OPT,
                                         "w", "Map Width", arg_metadata::INTEGER | arg_metadata::OPT,
                                         "h", "Map Height", arg_metadata::INTEGER | arg_metadata::OPT,
                                         array_arg);
    ///centre, n, w, h, and array

    ret["sys.view"].description = "Shows a map of the local system, from your perspective";
    ret["sys.view"].return_data = make_met("", "Ascii Map of the local System", arg_metadata::STRING, "", "Array of NPC Objects", arg_metadata::ARRAY);
    ret["sys.view"].param_data = make_met(
                                         "user", "User to display from", arg_metadata::USER | arg_metadata::OPT,
                                         "fit", "Should Fit Map", arg_metadata::BOOLEAN | arg_metadata::OPT,
                                         "n", "Number of NPCs depthwise to display from location", arg_metadata::INTEGER | arg_metadata::OPT,
                                         "w", "Map Width", arg_metadata::INTEGER | arg_metadata::OPT,
                                         "h", "Map Height", arg_metadata::INTEGER | arg_metadata::OPT,
                                         "scale", "Map Scale", arg_metadata::NUMERIC | arg_metadata::OPT,
                                         array_arg);

    ret["sys.move"].description = "Moves you from A to B in a system";
    ret["sys.move"].param_data = make_met(
                                            "to", "User to move to, or array of [x, y, z]", arg_metadata::USER
                                         );

    ret["sys.access"].description = "Accesses a users control panel. Can be used to claim NPCs";
    ret["sys.access"].param_data = make_met(
                                                "user", "User to access", arg_metadata::USER
                                           );
    ret["sys.access"].requires_breach = true;

    ret["sys.limits"].description = "Shows a list of limits due to seclevels";
    ret["sys.limits"].return_data = make_met("", "Description of seclevel limits", arg_metadata::STRING, "", "Object containing seclevel limits", arg_metadata::ARRAY);
    ret["sys.limits"].param_data = make_met("sys", "Target System", arg_metadata::STRING | arg_metadata::OPT,
                                            "user", "Target User", arg_metadata::USER | arg_metadata::OPT,
                                            array_arg);

    return ret;
}

bool requested_scripting_api(js::value& arg)
{
    return arg["array"].is_truthy();
}

std::string prettify_chat_strings(std::vector<nlohmann::json>& found, bool use_channels)
{
    std::string str;

    ///STD::CHRONO PLS
    for(int kk=0; kk < (int)found.size(); kk++)
    {
        nlohmann::json& i = found[kk];

        size_t time_code_ms = 0;

        if(i.find("time_ms") != i.end())
        {
            if(i["time_ms"].is_number())
                time_code_ms = (size_t)i["time_ms"];
            else if(i["time_ms"].is_string())
            {
                std::string str = i["time_ms"];
                time_code_ms = std::stoll((std::string)str);
            }
        }

        std::string channel;
        std::string usrname;
        std::string msg;

        if(i.find("channel") != i.end())
            channel = i["channel"];

        if(i.find("user") != i.end())
            usrname = i["user"];

        if(i.find("msg") != i.end())
            msg = i["msg"];

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

js::value cash__balance(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    mongo_nolock_proxy mongo_user_info = get_global_mongo_user_info_context(get_thread_id(vctx));

    user usr;
    usr.load_from_db(mongo_user_info, get_caller(vctx));

    return js::make_value(vctx, usr.cash);
}

js::value scripts__get_level(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    if(!arg.has("name"))
        return js::make_error(vctx, "Call with name:\"scriptname\"");

    std::string str = arg["name"];

    std::string script_err;

    unified_script_info script = unified_script_loading(get_thread_id(vctx), str, script_err);

    if(!script.valid)
        return js::make_error(vctx, script_err);

    return js::make_value(vctx, script.seclevel);
}

std::string format_pretty_names(const std::vector<std::string>& names, bool colour, bool all_characters = false)
{
    std::string ret;

    for(int i=0; i < (int)names.size(); i++)
    {
        std::string name = names[i];

        if(colour)
        {
            std::vector<std::string> post_split = no_ss_split(name, ".");

            if(post_split.size() > 0)
            {
                name = "";

                for(int i=0; i < (int)post_split.size(); i++)
                {
                    if(i == 0)
                    {
                        if(!all_characters)
                            name += colour_string_only_alnum(post_split[i]);
                        else
                            name += colour_string(post_split[i]);
                    }
                    else
                    {
                        name += post_split[i];
                    }

                    if(i != (int)post_split.size() - 1)
                    {
                        name += ".";
                    }
                }
            }
        }

        if(i != (int)names.size()-1)
            ret.append(name + "\n");
        else
            ret.append(name);
    }

    return ret;
}

std::string format_pretty_names(const std::vector<user_log>& names, bool colour, bool all_characters = false)
{
    std::vector<std::string> text;

    for(const user_log& i : names)
    {
        text.push_back(i.fetch());
    }

    return format_pretty_names(text,colour, all_characters);
}

js::value scripts__me(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    bool make_array = requested_scripting_api(arg);

    std::string usr = get_caller(vctx);

    user loaded_user;

    {
        mongo_nolock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        loaded_user.load_from_db(user_ctx, usr);
    }

    std::vector<std::string> names;
    mongo_lock_proxy item_context = get_global_mongo_user_items_context(get_thread_id(vctx));

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

            names.push_back("#" + (std::string)req.get_prop("item_id"));
        }
    }

    {
        mongo_requester request;
        request.set_prop("owner", usr);
        request.set_prop("full", 1);
        request.set_prop("item_type", (int)item_types::EMPTY_SCRIPT_BUNDLE);

        std::vector<mongo_requester> results = request.fetch_from_db(item_context);

        for(mongo_requester& req : results)
        {
            if(req.get_prop("registered_as") == "")
                continue;

            std::string item_id = req.get_prop("item_id");

            if(!loaded_user.has_loaded_item(item_id))
                continue;

            std::string name = usr + "." + (std::string)req.get_prop("registered_as") + " `D[bundle]`";

            names.push_back(name);
        }
    }

    if(make_array)
    {
        return js::make_value(vctx, names);
    }
    else
    {
        std::string str = format_pretty_names(names, true);

        return js::make_value(vctx, str);
    }
}

///should take a pretty:1 argument

js::value scripts__public(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int pretty = !requested_scripting_api(arg);
    int seclevel = arg.has("sec") ? arg["sec"] : -1;

    mongo_requester request;
    request.set_prop("is_script", 1);
    request.set_prop("in_public", 1);

    if(seclevel >= 0 && seclevel <= 4)
        request.set_prop("seclevel", seclevel);

    request.set_prop_sort_on("item_id", 1);

    ///seclevel
    //request.set_prop("seclevel", num);
    //request.set_prop("in_public", 1"; ///TODO: FOR WHEN YOU CAN UP PUBLIC

    mongo_nolock_proxy item_context = get_global_mongo_user_items_context(get_thread_id(vctx));

    std::vector<mongo_requester> results = request.fetch_from_db(item_context);

    std::vector<std::string> names;

    for(mongo_requester& req : results)
    {
        names.push_back("#" + (std::string)req.get_prop("item_id"));
    }

    if(pretty)
    {
        std::string str = format_pretty_names(names, true);

        return js::make_value(vctx, str);
    }
    else
    {
        return js::make_value(vctx, names);
    }
}

js::value scripts__info(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string script;

    if(arg.is_string())
        script = (std::string)arg;
    else if(arg.is_object())
        script = (std::string)arg["name"];
    else
        return js::make_error(vctx, "Expected {name:\"script.name\"}");

    bool is_arr = requested_scripting_api(arg);

    std::string usi_error;

    unified_script_info usi = unified_script_loading(get_thread_id(vctx), script, usi_error);

    if(!is_arr)
    {
        if(usi.valid)
        {
            return js::make_value(vctx, usi.get_autogenerated_documentation(false));
        }
        else
        {
            return js::make_value(vctx, "No help available");
        }
    }
    else
    {
        nlohmann::json ret;

        std::vector<std::string> keys;

        for(int i=0; i < (int)usi.metadata.param_data.size(); i++)
        {
            arg_metadata& params = usi.metadata.param_data[i];
            //arg_metadata& rets = usi.metadata.return_data[i];

            keys.push_back(params.key_name);
        }

        std::string description = usi.metadata.description;
        bool requires_breach = usi.metadata.requires_breach;

        ret["keys"] = keys;
        ret["description"] = description;
        ret["requires_breach"] = requires_breach;

        return js::make_value(vctx, ret);
    }
}

js::value cash_internal_xfer(js::value_context& vctx, const std::string& from, const std::string& to, double amount, bool pvp_action)
{
    if(amount < 0 || amount >= pow(2, 32))
    {
        return js::make_error(vctx, "Amount error");
    }

    ///this is considered a catastrophically large amount
    double cash_to_destroy_link = 10000;

    if(from == to)
    {
        return js::make_success(vctx);
    }

    #ifdef XFER_PATHS
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    std::vector<std::string> path = playspace_network_manage.get_accessible_path_to(vctx, to, from, (path_info::path_info)(path_info::NONE | path_info::ALLOW_WARP_BOUNDARY | path_info::TEST_ACTION_THROUGH_WARP_NPCS), -1, amount / cash_to_destroy_link);

    if(path.size() == 0)
        return js::make_error(vctx, "No path to user through the network");

    //std::string leak_msg = "Xfer'd " + std::to_string(amount);

    user_log next;
    //next.add("cash_xfer", std::to_string(amount), "");
    next.add("type", "cash_xfer", "X");
    next.add("amount", std::to_string(amount), "");

    playspace_network_manage.modify_path_per_link_strength_with_logs(path, -amount / cash_to_destroy_link, {next}, get_thread_id(vctx));
    #endif // 0

    size_t current_time = get_wall_time();

    {
        mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(get_thread_id(vctx));

        user destination_usr;

        if(!destination_usr.load_from_db(mongo_user_info, to))
        {
            return js::make_error(vctx, "User does not exist");
        }

        user from_user;

        if(!from_user.load_from_db(mongo_user_info, from))
        {
            return js::make_error(vctx, "From user does not exist");
        }

        #ifdef SECLEVEL_FUNCTIONS
        ///TODO ALARM:
        ///need to make it so that when you xfer cash to someone it updates how much can be stolen from them
        if(!pvp_action)
        {
            size_t current_time = get_wall_time();

            low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

            std::optional<low_level_structure*> from_system_opt = low_level_structure_manage.get_system_of(from);
            std::optional<low_level_structure*> to_system_opt = low_level_structure_manage.get_system_of(to);

            if(!from_system_opt.has_value() || !to_system_opt.has_value())
                return js::make_error(vctx, "Should be impossible (i love podracing)");

            low_level_structure& from_system = *from_system_opt.value();
            low_level_structure& to_system = *to_system_opt.value();

            if(&from_system != &to_system)
            {
                user_limit& lim = from_user.user_limits[user_limit::CASH_SEND];

                double real_cash_limit = from_user.get_max_sendable_cash(current_time, from_system, to_system);

                //std::cout << "real cash limit " << real_cash_limit << std::endl;

                if(real_cash_limit < amount)
                    return js::make_error(vctx, "Cannot send due to seclevel limits, check #sys.limits({user:\"" + to + "\"})");

                //if(fabs(real_cash_limit) < 0.0001)
                //    return push_error(ctx, "Some sort of calculation error (cash xfer seclevel rlimit)");

                if(fabs(real_cash_limit) < 0.0001)
                    return js::make_error(vctx, "Cash xfer limit < 0.0001");

                double fraction_removed = amount / get_most_secure_seclevel_of(from_system, to_system).get_ratelimit_max_cash_send();

                lim.data = clamp(lim.calculate_current_data(current_time) - fraction_removed, 0., 1.);
                lim.time_at = current_time;
            }

            ///work out old cash
            ///work out stolen cash
            ///add new cash to dest, and remove from from
            ///update pvp balances for both
        }
        ///this is the pvp case
        ///must be in the same system to steal so no need to find both systems
        ///i think this is incorrect as it isn't handling the 50% of 50% problem
        ///need to calculate fraction properly assuming the original amount of cash
        ///and then work out if thats correct

        ///need to update pvp balance for person receiving cash
        else
        {
            low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

            std::optional<low_level_structure*> from_system_opt = low_level_structure_manage.get_system_of(from);

            if(!from_system_opt.has_value())
                return js::make_error(vctx, "Target is not in a system (from) (insert anakin sand meme)");

            user_limit& lim = from_user.user_limits[user_limit::CASH_STEAL];

            low_level_structure& from_system = *from_system_opt.value();

            double max_stealable = from_user.get_max_stealable_cash(current_time, from_system);
            double old_cash = from_user.get_pvp_old_cash_estimate(current_time);

            if(max_stealable < amount)
                return js::make_error(vctx, "Cannot steal due to seclevel limits, check #cash.expose({user:\"" + from + "\"})");

            if(old_cash > 0.1)
            {
                double fraction_removed = amount / old_cash;

                lim.data = clamp(lim.calculate_current_data(current_time) - fraction_removed, 0., 1.);
                lim.time_at = current_time;
            }
        }
        #endif // SECLEVEL_FUNCTIONS

        double remaining = from_user.cash - amount;

        if(remaining < 0)
        {
            return js::make_error(vctx, "Can't send this amount");
        }

        ///need to check destination usr can handle amount
        from_user.cash -= amount;
        destination_usr.cash += amount;

        from_user.overwrite_user_in_db(mongo_user_info);
        destination_usr.overwrite_user_in_db(mongo_user_info);
    }

    {
        //std::string cash_log = "`XCash xfer` | from: " + from  + ", to: " + to + ", amount: " + std::to_string(amount);

        user_log next;
        next.add("type", "cash_xfer", "X");
        next.add("from", from, string_to_colour(from));
        next.add("to", to, string_to_colour(to));
        next.add("amount", std::to_string(amount), "");

        auto ret_opt = make_logs_on(vctx, from, user_node_info::CASH_SEG, {next});

        if(ret_opt.has_value())
            return ret_opt.value();

        auto ret_opt2 = make_logs_on(vctx, to, user_node_info::CASH_SEG, {next});

        if(ret_opt2.has_value())
            return ret_opt2.value();
    }

    if(!pvp_action)
    {
        quest_manager& qm = get_global_quest_manager();

        quest_cash_send_data dat;
        dat.target = to;
        dat.at_least = amount;

        qm.process(get_thread_id(vctx), from, dat);
    }

    create_xfer_notif(vctx, from, to, amount);

    return js::make_success(vctx);
}

///TODO: TRANSACTION HISTORY
js::value cash__xfer_to(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    RATELIMIT_VDUK(CASH);

    if(!arg["user"].is_string())
        return js::make_error(vctx, "Call with user:\"usr\"");

    std::string destination_name = arg["user"];

    double amount = 0;

    if(!arg["amount"].is_number())
        return js::make_error(vctx, "Only numbers supported atm");

    amount = arg;

    return cash_internal_xfer(vctx, get_caller(vctx), destination_name, amount, false);
}

js::value cash__xfer_to_caller(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    return js::make_error(vctx, "deprecated, use #os.cash.xfer_to");
}

///this is only valid currently, will need to expand to hardcode in certain folders

js::value scripts__core(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int make_array = requested_scripting_api(arg);

    std::vector<std::string> names;

    for(auto& i : privileged_functions)
    {
        if(std::find(hidden_functions.begin(), hidden_functions.end(), i.first) != hidden_functions.end())
            continue;

        names.push_back("#" + i.first);
    }

    if(make_array)
    {
        return js::make_value(vctx, names);
    }
    else
    {
        std::string str = format_pretty_names(names, false);

        return js::make_value(vctx, str);
    }
}

size_t get_wall_time();
double get_wall_time_s();


bool user_in_channel(mongo_lock_proxy& mongo_ctx, const std::string& username, const std::string& channel)
{
    mongo_requester request;
    request.set_prop("channel_name", channel);

    auto found = request.fetch_from_db(mongo_ctx);

    if(found.size() != 1)
        return false;

    auto channel_users = (std::vector<std::string>)found[0].get_prop("user_list");

    return array_contains(channel_users, username);
}


bool is_valid_channel_name(const std::string& in)
{
    if(in.size() == 0)
        return false;

    if(in.size() > 16)
        return false;

    for(auto& i : in)
    {
        if(isalnum(i) || i == '_')
            continue;

        if(i == ' ')
            return false;

        return false;
    }

    return true;
}

js::value channel__create(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    RATELIMIT_VDUK(CREATE_CHANNEL);

    std::string chan = arg["name"];
    std::string password = arg["password"];

    if(password.size() > 16)
        password.resize(16);

    if(!is_valid_channel_name(chan))
        return js::make_error(vctx, "Invalid Name");

    mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

    mongo_requester request;
    request.set_prop("channel_name", chan);

    if(request.fetch_from_db(mongo_ctx).size() > 0)
        return js::make_error(vctx, "Channel already exists");

    std::vector<std::string> user_list{get_caller(vctx)};

    mongo_requester to_insert;
    to_insert.set_prop("channel_name", chan);
    to_insert.set_prop("password", password);
    to_insert.set_prop("user_list", user_list);

    to_insert.insert_in_db(mongo_ctx);

    return js::make_success(vctx);
}

js::value channel__join(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string chan = arg["name"];
    std::string password = arg["password"];

    if(password.size() > 16)
        password.resize(16);

    if(!is_valid_channel_name(chan))
        return js::make_error(vctx, "Invalid Name");

    mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

    mongo_requester request;
    request.set_prop("channel_name", chan);

    std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

    if(found.size() == 0)
        return js::make_error(vctx, "Channel does not exist");

    if(found.size() > 1)
        return js::make_error(vctx, "Some kind of catastrophic error: Yellow Sparrow");

    mongo_requester& found_channel = found[0];

    if(found_channel.has_prop("password") && found_channel.get_prop("password") != password)
        return js::make_error(vctx, "Wrong Password");

    std::vector<std::string> users = (std::vector<std::string>)found_channel.get_prop("user_list");

    std::string username = get_caller(vctx);

    if(array_contains(users, username))
        return js::make_success(vctx);

    users.push_back(username);

    mongo_requester to_find = request;

    mongo_requester to_set;
    to_set.set_prop("user_list", users);

    to_find.update_in_db_if_exact(mongo_ctx, to_set);

    return js::make_success(vctx);
}

js::value channel__list(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    bool is_arr = requested_scripting_api(arg);

    std::string username = get_caller(vctx);

    std::vector<std::string> ret;

    mongo_requester all;
    all.exists_check["channel_name"] = 1;

    std::vector<mongo_requester> found;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(-2);
        found = all.fetch_from_db(mongo_ctx);
    }

    for(auto& i : found)
    {
        std::vector<std::string> users = (std::vector<std::string>)i.get_prop("user_list");

        for(auto& k : users)
        {
            if(k == username)
            {
                ret.push_back(i.get_prop("channel_name"));
                break;
            }
        }
    }

    if(is_arr)
    {
        return js::make_value(vctx, ret);
    }
    else
    {
        std::string str;

        for(auto& i : ret)
        {
            str += i + "\n";
        }

        return js::make_value(vctx, strip_trailing_newlines(str));
    }
}

js::value channel__leave(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string chan = arg["name"];

    if(!is_valid_channel_name(chan))
        return js::make_error(vctx, "Invalid Name");

    mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

    mongo_requester request;
    request.set_prop("channel_name", chan);

    std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

    if(found.size() == 0)
        return js::make_error(vctx, "Channel does not exist");

    if(found.size() > 1)
        return js::make_error(vctx, "Some kind of catastrophic error: Yellow Sparrow");

    mongo_requester& found_channel = found[0];

    std::vector<std::string> users = (std::vector<std::string>)found_channel.get_prop("user_list");

    std::string username = get_caller(vctx);

    if(!array_contains(users, username))
        return js::make_error(vctx, "Not in Channel");

    auto it = std::find(users.begin(), users.end(), username);

    if(it != users.end())
        users.erase(it);

    if(array_contains(users, username))
        return js::make_success(vctx);

    mongo_requester to_find = request;

    mongo_requester to_set;
    to_set.set_prop("user_list", users);

    to_find.update_in_db_if_exact(mongo_ctx, to_set);

    return js::make_success(vctx);
}

js::value msg__manage(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string to_join = arg["join"];
    std::string to_leave = arg["leave"];
    std::string to_create = arg["create"];

    int num_set = 0;

    if(to_join.size() > 0)
    {
        if(!is_valid_channel_name(to_join))
            return js::make_error(vctx, "Invalid Name");

        num_set++;
    }

    if(to_leave.size() > 0)
    {
        if(!is_valid_channel_name(to_leave))
            return js::make_error(vctx, "Invalid Name");

        num_set++;
    }

    if(to_create.size() > 0)
    {
        if(!is_valid_channel_name(to_create))
            return js::make_error(vctx, "Invalid Name");

        RATELIMIT_VDUK(CREATE_CHANNEL);

        num_set++;
    }

    if(num_set != 1)
        return js::make_error(vctx, "Only one leave/join/create parameter may be specified");

    if(to_join.size() >= 10 || to_leave.size() >= 10 || to_create.size() >= 10)
        return js::make_error(vctx, "Invalid Leave/Join/Create arguments");

    std::string username = get_caller(vctx);

    bool joining = to_join != "";

    if(to_join.size() > 0 || to_leave.size() > 0)
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

        mongo_requester request;

        if(to_join != "")
            request.set_prop("channel_name", to_join);
        if(to_leave != "")
            request.set_prop("channel_name", to_leave);

        std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

        if(found.size() == 0)
            return js::make_error(vctx, "Channel does not exist");

        if(found.size() > 1)
            return js::make_error(vctx, "Some kind of catastrophic error: Yellow Sparrow");

        mongo_requester& chan = found[0];

        std::vector<std::string> users = (std::vector<std::string>)chan.get_prop("user_list");

        if(joining && array_contains(users, username))
            return js::make_error(vctx, "In channel");

        if(!joining && !array_contains(users, username))
            return js::make_error(vctx, "Not in Channel");

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
        to_set.set_prop("user_list", users);

        to_find.update_in_db_if_exact(mongo_ctx, to_set);
    }

    if(to_create.size() > 0)
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

        mongo_requester request;
        request.set_prop("channel_name", to_create);

        if(request.fetch_from_db(mongo_ctx).size() > 0)
            return js::make_error(vctx, "Channel already exists");

        mongo_requester to_insert;
        to_insert.set_prop("channel_name", to_create);

        to_insert.insert_in_db(mongo_ctx);
    }

    return js::make_success(vctx);
}

std::vector<std::string> get_users_in_channel(mongo_lock_proxy& mongo_ctx, const std::string& channel)
{
    mongo_requester request;
    request.set_prop("channel_name", channel);

    auto found = request.fetch_from_db(mongo_ctx);

    if(found.size() != 1)
        return {};

        //return push_error(ctx, "Something real weird happened: Orange Canary");

    mongo_requester& chan = found[0];

    std::vector<std::string> users = (std::vector<std::string>)chan.get_prop("user_list");

    return users;
}

js::value msg__send(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    RATELIMIT_VDUK(CHAT);

    std::string channel = arg["channel"];
    std::string msg = arg["msg"];

    if(channel == "")
        channel = "global";

    if(channel == "" || msg == "" || channel.size() >= 10 || msg.size() >= 10000)
    {
        js::make_error(vctx, "Usage: #hs.msg.send({channel:\"<name>\", msg:\"msg\"})");
    }

    user my_user;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!my_user.load_from_db(mongo_ctx, get_caller(vctx)))
            return js::make_error(vctx, "No such user");
    }

    channel = strip_whitespace(channel);

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    std::vector<std::string> users;

    {
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

            if(!user_in_channel(mongo_ctx, get_caller(vctx), channel))
                return js::make_error(vctx, "User not in channel or doesn't exist");
        }

        if(channel != "local")
        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));
            users = get_users_in_channel(mongo_ctx, channel);
        }
        else
        {
            std::optional<low_level_structure*> system_opt = low_level_structure_manage.get_system_of(get_caller(vctx));

            if(!system_opt.has_value())
                return js::make_error(vctx, "Dust is coarse and irritating and gets everywhere (no system)");

            low_level_structure& structure = *system_opt.value();

            users = structure.get_all_users();

            mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

            for(int i=0; i < (int)users.size(); i++)
            {
                if(!user_in_channel(mongo_ctx, users[i], channel))
                {
                    users.erase(users.begin() + i);
                    i--;
                    continue;
                }
            }
        }
    }

    bool found = false;

    for(auto& i : users)
    {
        if(i == get_caller(vctx))
        {
            found = true;
            break;
        }
    }

    if(!found)
        return js::make_error(vctx, "Not in channel");

    {
        ///TODO: LIMIT
        for(auto& current_user : users)
        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(get_thread_id(vctx));
            mongo_ctx.change_collection(current_user);

            size_t real_time = get_wall_time();

            nlohmann::json to_insert;
            to_insert["user"] = get_caller(vctx);
            to_insert["is_chat"] = 1;
            to_insert["msg"] = msg;
            to_insert["channel"] = channel;
            to_insert["time_ms"] = real_time;
            to_insert["processed"] = 0;

            insert_in_db(mongo_ctx, to_insert);
        }
    }

    command_handler_state* found_ptr = js::get_heap_stash(vctx)["command_handler_state_pointer"].get_ptr<command_handler_state>();

    if(found_ptr && get_caller_stack(vctx).size() > 0 && get_caller_stack(vctx)[0] == found_ptr->get_user_name())
        send_async_message(vctx, handle_client_poll_json(my_user).dump());

    return js::make_success(vctx);
}


js::value msg__tell(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    RATELIMIT_VDUK(CHAT);

    std::string to = arg["user"];
    std::string msg = arg["msg"];

    if(to == "")
        return js::make_error(vctx, "Invalid user argument");

    if(msg.size() > 10000)
        return js::make_error(vctx, "Too long msg, 10k is max");

    if(!get_user(to, get_thread_id(vctx)))
        return js::make_error(vctx, "Invalid User");

    mongo_lock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(get_thread_id(vctx));
    mongo_ctx.change_collection(to);

    size_t real_time = get_wall_time();

    nlohmann::json to_insert;
    to_insert["user"] = get_caller(vctx);
    to_insert["is_tell"] = 1;
    to_insert["msg"] = msg;
    to_insert["time_ms"] = real_time;
    to_insert["processed"] = 0;

    insert_in_db(mongo_ctx, to_insert);

    return js::make_success(vctx);
}

void create_notification(int lock_id, const std::string& to, const std::string& notif_msg)
{
    //COOPERATE_KILL();

    if(to == "")
        return;

    if(notif_msg.size() > 10000)
        return;

    mongo_lock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(lock_id);
    mongo_ctx.change_collection(to);

    size_t real_time = get_wall_time();

    nlohmann::json to_insert;
    to_insert["user"] = to;
    to_insert["is_notif"] = 1;
    to_insert["msg"] = notif_msg;
    to_insert["time_ms"] = real_time;
    to_insert["processed"] = 0;

    insert_in_db(mongo_ctx, to_insert);
}

void create_xfer_notif(js::value_context& vctx, const std::string& xfer_from, const std::string& xfer_to, double amount)
{
    std::string notif_from = make_notif_col("-Sent " + to_string_with_enforced_variable_dp(amount, 2) + " (xfer)-");
    std::string notif_to = make_notif_col("-Received " + to_string_with_enforced_variable_dp(amount, 2) + " (xfer)-");

    if(xfer_from != "")
        create_notification(get_thread_id(vctx), xfer_from, notif_from);

    if(xfer_to != "")
        create_notification(get_thread_id(vctx), xfer_to, notif_to);
}

void create_xfer_item_notif(js::value_context& vctx, const std::string& xfer_from, const std::string& xfer_to, const std::string& item_name)
{
    std::string notif_from = make_notif_col("-Lost " + item_name + " (xfer)-");
    std::string notif_to = make_notif_col("-Received " + item_name + " (xfer)-");

    if(xfer_from != "")
        create_notification(get_thread_id(vctx), xfer_from, notif_from);

    if(xfer_to != "")
        create_notification(get_thread_id(vctx), xfer_to, notif_to);
}

void create_destroy_item_notif(js::value_context& vctx, const std::string& to, const std::string& item_name)
{
    if(item_name == "")
        return;

    std::string cull_msg = make_notif_col("-Destroyed " + item_name + "-");

    create_notification(get_thread_id(vctx), to, cull_msg);
}

std::string format_time(const std::string& in)
{
    if(in.size() == 1)
        return "0" + in;

    if(in.size() == 0)
        return "00";

    return in;
}

js::value msg__recent(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string channel = arg["channel"];
    int num = arg.has("count") ? arg["count"] : -1;
    bool pretty = !requested_scripting_api(arg);
    bool is_tell = arg["tell"].is_truthy();

    if(num <= 0)
        num = 10;

    if(channel.size() == 0)
        channel = "global";

    if(num >= 100)
        return js::make_error(vctx, "Count cannot be >= than 100");

    if(!is_tell)
    {
        if(!is_valid_channel_name(channel))
            return js::make_error(vctx, "Invalid channel name");

        if(channel == "" || channel.size() >= 10)
            return js::make_error(vctx, "Usage: #ms.msg.recent({channel:\"<name>\", count:num, pretty:1})");
    }

    if(channel.size() > 50)
        channel.resize(50);

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(vctx));

        if(!user_in_channel(mongo_ctx, get_caller(vctx), channel))
            return js::make_error(vctx, "User not in channel or doesn't exist");
    }

    mongo_lock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(get_thread_id(vctx));
    mongo_ctx.change_collection(get_caller(vctx));

    ///ALARM: ALARM: RATE LIMIT
    nlohmann::json request;

    if(!is_tell)
    {
        request["channel"] =  channel;
        request["is_chat"] = 1;
    }
    else
    {
        request["is_tell"] = 1;
    }

    nlohmann::json time_opt;
    time_opt["time_ms"] = -1;

    nlohmann::json opt;
    opt["sort"] = time_opt;
    opt["limit"] = num;

    std::vector<nlohmann::json> found = fetch_from_db(mongo_ctx, request, opt);

    for(nlohmann::json& j : found)
    {
        if(j.find("_id") != j.end())
        {
            j.erase(j.find("_id"));
        }
    }

    if(!pretty)
    {
        return js::make_value(vctx, found);
    }
    else
    {
        std::string str = prettify_chat_strings(found, !is_tell);

        return js::make_value(vctx, str);
    }
}


js::value users__me(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int pretty = !requested_scripting_api(arg);

    std::string caller = get_caller(vctx);

    user current_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!current_user.exists(mongo_ctx, caller))
            return js::make_error(vctx, "Should be impossible (users.me no user)");

        current_user.load_from_db(mongo_ctx, caller);
    }

    mongo_nolock_proxy mongo_ctx = get_global_mongo_global_properties_context(get_thread_id(vctx));

    std::string auth_token = current_user.get_auth_token_binary();

    auth user_auth;
    user_auth.load_from_db(mongo_ctx, auth_token);

    std::vector names = user_auth.users;

    ///users in user db don't know about the other users
    ///and we can't perform a query across multiple collections, quite rightly
    ///so have to revisit updating auth
    if(pretty)
    {
        std::string str = format_pretty_names(names, true, true);

        return js::make_value(vctx, str);
    }
    else
    {
        return js::make_value(vctx, names);
    }
}

js::value users__accessible(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int pretty = !requested_scripting_api(arg);

    std::string caller = get_caller(vctx);

    user current_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!current_user.exists(mongo_ctx, caller))
            return js::make_error(vctx, "Should be impossible (users.accessible no user)");

        current_user.load_from_db(mongo_ctx, caller);
    }

    std::vector<std::string> names = current_user.users_i_have_access_to;

    ///users in user db don't know about the other users
    ///and we can't perform a query across multiple collections, quite rightly
    ///so have to revisit updating auth
    if(pretty)
    {
        std::string str = format_pretty_names(names, true);

        return js::make_value(vctx, str);
    }
    else
    {
        return js::make_value(vctx, names);
    }
}

#if 0
///pretty tired when i wrote this check it for mistakes

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


std::string escape_str(std::string in)
{
    for(int i=0; i < (int)in.size(); i++)
    {
        if(in[i] == '\n')
        {
            in[i] = '\\';
            in.insert(in.begin() + i + 1, 'n');
        }
    }

    return in;
}


std::string format_item(item& i, bool is_short, user& usr, user_nodes& nodes)
{
    if(is_short)
    {
        std::string str = i.get_prop("short_name");

        if(nodes.any_contains_lock(i.item_id))
        {
            str += " [on_node]";
        }

        return str;
    }

    std::string ret = "{\n";

    bool is_open_source = (int)i.get("open_source");

    //for(auto& p : i.data)
    for(auto it = i.data.begin(); it != i.data.end(); it++)
    {
        if(it.key() == "_id")
            continue;

        if(it.key() == "_cid")
            continue;

        if(!is_open_source && it.key() == "unparsed_source")
            continue;

        if(!is_open_source && it.key() == "parsed_source")
            continue;

        std::string str = i.get_stringify(it.key());

        #ifdef DO_DESC_ESCAPING
        if(it.key() == "desc")
        {
            str = escape_str(str);
        }
        #endif // DO_DESC_ESCAPING

        ret += "    " + it.key() + ": " + str + ",\n";
    }

    if(usr.has_loaded_item(i.item_id))
        ret += "    loaded: true\n";

    if(nodes.any_contains_lock(i.item_id))
        ret += "    on_node: true";

    return ret + "}";
}


nlohmann::json get_item_raw(item& i, bool is_short, user& usr, user_nodes& nodes)
{
    nlohmann::json obj;

    if(usr.has_loaded_item(i.item_id))
        obj["loaded"] = true;

    if(nodes.any_contains_lock(i.item_id))
        obj["on_node"] = true;

    if(is_short)
    {
        obj["short_name"] = i.get_prop("short_name");

        return obj;
    }

    bool is_open_source = (int)i.get("open_source");

    //for(auto& p : i.props.properties)
    for(auto it = i.data.begin(); it != i.data.end(); it++)
    {
        if(it.key() == "_id")
            continue;

        if(!is_open_source && it.key() == "unparsed_source")
            continue;

        if(!is_open_source && it.key() == "parsed_source")
            continue;

        if(is_short && it.key() != "short_name")
            continue;

        obj[it.key()] = i.get_stringify(it.key());
    }

    return obj;
}

/*
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


std::string load_item_raw(int node_idx, int load_idx, int unload_idx, user& usr, user_nodes& nodes, std::string& accum, int thread_id)
{
    std::string to_load = usr.index_to_item(load_idx);
    std::string to_unload = usr.index_to_item(unload_idx);

    std::string which = to_load;

    if(to_unload.size() > 0)
        which = to_unload;

    if(which == "")
        return "Item not found";

    item next;

    {
        mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        next.item_id = which;

        if(!db_disk_exists(item_ctx, next))
            return "Something weird happened";

        db_disk_load(item_ctx, next, which);
    }

    if((int)next.get("item_type") != (int)item_types::LOCK)
    {
        std::string tl = usr.index_to_item(load_idx);
        std::string tul = usr.index_to_item(unload_idx);

        ///NEED TO CHECK CONSTRAINTS HERE ALARM
        ///ALARM ALARM NEED TO PREVENT UNLOADABLE ITEMS FROM BEING LOADED!!!
        usr.load_item(tl);
        usr.unload_item(tul);

        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(thread_id);

            usr.overwrite_user_in_db(mongo_ctx);
        }

        accum += "Success";

        return "";
    }

    if(which == to_load && node_idx == -1)
    {
        if(nodes.any_contains_lock(to_load))
            return "Already loaded";

        {
            mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

            item lock;
            db_disk_load(item_ctx, lock, to_load);
            lock.breach();
            db_disk_overwrite(item_ctx, lock);

            nodes.load_lock_to_any(item_ctx, to_load);
        }

        accum += "Loaded\n";
    }

    if(which == to_load && node_idx != -1)
    {
        user_node* node = nodes.type_to_node((user_node_t)node_idx);

        {
            mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

            if(node->can_load_lock(item_ctx, to_load))
            {
                item lock;
                db_disk_load(item_ctx, lock, to_load);
                lock.breach();
                db_disk_overwrite(item_ctx, lock);

                node->load_lock(to_load);
            }
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

    if(which == to_unload && node_idx != -1)
    {
        user_node* node = nodes.type_to_node((user_node_t)node_idx);

        node->unload_lock(to_load);

        accum += "Unloaded\n";
    }

    //if(which == to_unload && node_idx )

    {
        mongo_nolock_proxy node_ctx = get_global_mongo_node_properties_context(thread_id);
        nodes.overwrite_in_db(node_ctx);
    }

    return "";
}


js::value push_internal_items_view(js::value_context& vctx, int pretty, int full, user_nodes& nodes, user& found_user, std::string preamble, bool pvp)
{
    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    auto sys_opt = low_level_structure_manage.get_system_of(found_user);

    if(!sys_opt.has_value())
        return js::make_error(vctx, "Lost in push items internal");

    low_level_structure& sys = *sys_opt.value();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

    std::vector<std::string> to_ret = found_user.upgr_idx;

    size_t current_time = get_wall_time();

    if(pretty)
    {
        std::string formatted;

        if(pvp)
        {
            formatted = "available: ";

            int currently_stealable = found_user.get_max_stealable_items(current_time, sys);

            formatted += std::to_string(currently_stealable) + "\n";
        }

        if(full)
           formatted = "[\n";

        int idx = 0;

        for(std::string& item_id : to_ret)
        {
            item next;
            db_disk_load(mongo_ctx, next, item_id);

            if(!full)
            {
                if(found_user.has_loaded_item(next.item_id))
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

        std::string res = preamble + formatted;

        while(res.size() > 0 && res.back() == '\n')
            res.pop_back();

        return js::make_value(vctx, res);
    }
    else
    {
        int currently_stealable = found_user.get_max_stealable_items(current_time, sys);

        nlohmann::json ret;

        std::vector<nlohmann::json> objs;
        int index = 0;

        for(std::string& item_id : to_ret)
        {
            item next;
            db_disk_load(mongo_ctx, next, item_id);

            auto data = get_item_raw(next, !full, found_user, nodes);
            data["idx"] = index++;

            objs.push_back(data);
        }

        if(pvp)
        {
            ret["available"] = currently_stealable;
            ret["exposed"] = objs;

            return js::make_value(vctx, ret);
        }
        else
        {
            return js::make_value(vctx, objs);
        }
    }
}

std::pair<bool, std::vector<int>> check_get_index_property(js::value& arg)
{
    if(!arg.has("idx"))
        return {false, std::vector<int>()};

    bool is_arr = arg.is_array();

    if(is_arr)
    {
        return {is_arr, (std::vector<int>)arg["idx"]};
    }
    else
    {
        int val = arg["idx"];

        return {is_arr, {val}};
    }
}

js::value item__cull(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    auto [is_arr, indices] = check_get_index_property(arg);

    if(indices.size() == 0)
        return js::make_error(vctx, "Index must be number or array of numbers, eg idx:0 or idx:[1, 2, 3]");

    auto opt_user_and_nodes = get_user_and_nodes(get_caller(vctx), get_thread_id(vctx));

    if(!opt_user_and_nodes.has_value())
        return js::make_error(vctx, "Catastrophic error Blue Walrus in item.cull");

    user& usr = opt_user_and_nodes->first;

    int offset = 0;

    std::vector<js::value> returns;

    std::sort(indices.begin(), indices.end(), [](const auto& i1, const auto& i2){return i1 > i2;});

    for(int idx : indices)
    {
        std::string id = usr.index_to_item(idx);

        if(id == "")
        {
            returns.push_back(js::make_error(vctx, "No such item"));
            continue;
        }

        std::string accum;

        auto ret = load_item_raw(-1, -1, idx, opt_user_and_nodes->first, opt_user_and_nodes->second, accum, get_thread_id(vctx));

        if(ret != "")
            returns.push_back(js::make_error(vctx, ret));
        else
            returns.push_back(js::make_success(vctx));

        {
            mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

            item found;
            db_disk_load(items_ctx, found, id);

            create_destroy_item_notif(vctx, get_caller(vctx), found.get_prop("short_name"));

            db_disk_remove(items_ctx, found);
        }

        usr.remove_item(id);
        offset++;
    }

    {
        mongo_nolock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        usr.overwrite_user_in_db(user_ctx);
    }

    if(is_arr)
        return js::make_value(vctx, returns);

    if(!is_arr && returns.size() == 1)
        return returns[0];

    return js::make_error(vctx, "Implementation error in item.cull");
}

js::value item__manage(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int pretty = !requested_scripting_api(arg);
    int full = arg["full"].is_truthy();

    int load_idx = arg.has("load") ? arg["load"] : -1;
    int unload_idx = arg.has("unload") ? arg["unload"] : -1;
    int node_idx = arg.has("node") ? arg["node"] : -1;

    std::string node_name = arg["node"];

    if(load_idx >= 0 && unload_idx >= 0)
        return js::make_error(vctx, "Only one load/unload at a time");

    std::string usage = "Usage: " + make_key_val("load", "idx") + " or " + make_key_val("unload", "idx") + ", and optionally " + make_key_val("node", "short_name");

    if(node_name != "")
    {
        for(int i=0; i < user_node_info::TYPE_COUNT; i++)
        {
            if(stolower(user_node_info::short_name[i]) == stolower(node_name))
            {
                node_idx = i;
                break;
            }
        }
    }

    user found_user;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!found_user.load_from_db(mongo_ctx, get_caller(vctx)))
            return js::make_error(vctx, "No such user/really catastrophic error");
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));

        nodes.ensure_exists(node_ctx, get_caller(vctx));
        nodes.load_from_db(node_ctx, get_caller(vctx));
    }

    if(load_idx >= 0 || unload_idx >= 0)
    {
        std::string accum;

        auto ret = load_item_raw(node_idx, load_idx, unload_idx, found_user, nodes, accum, get_thread_id(vctx));

        if(ret != "")
            return js::make_error(vctx, ret);

        return js::make_success(vctx);
    }

    return push_internal_items_view(vctx, pretty, full, nodes, found_user, usage + "\n", false);
}

js::value item__list(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    bool is_arr = requested_scripting_api(arg);
    bool full = arg["full"].is_truthy();

    std::optional<std::pair<user, user_nodes>> user_and_node_opt = get_user_and_nodes(get_caller(vctx), get_thread_id(vctx));

    if(!user_and_node_opt.has_value())
        return js::make_error(vctx, "User does not exist");

    return push_internal_items_view(vctx, !is_arr, full, user_and_node_opt->second, user_and_node_opt->first, "", false);
}

js::value item__load(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int node_idx = arg.has("node") ? arg["node"] : -1;

    auto [is_arr, indices] = check_get_index_property(arg);

    if(indices.size() == 0)
        return js::make_error(vctx, "Index must be number or array of numbers, eg idx:0 or idx:[1, 2, 3]");

    std::optional<std::pair<user, user_nodes>> user_and_node_opt = get_user_and_nodes(get_caller(vctx), get_thread_id(vctx));

    std::string node_name = arg["node"];

    if(!user_and_node_opt.has_value())
        return js::make_error(vctx, "User does not exist");

    if(node_name != "")
    {
        for(int i=0; i < user_node_info::TYPE_COUNT; i++)
        {
            if(stolower(user_node_info::short_name[i]) == stolower(node_name))
            {
                node_idx = i;
                break;
            }
        }
    }

    std::vector<js::value> rvector;

    for(int& idx : indices)
    {
        if(idx >= 0)
        {
            std::string accum;

            auto ret = load_item_raw(node_idx, idx, -1, user_and_node_opt->first, user_and_node_opt->second, accum, get_thread_id(vctx));

            if(ret != "")
                rvector.push_back(js::make_error(vctx, ret + " for index " + std::to_string(idx)));
            else
                rvector.push_back(js::make_success(vctx));
        }
        else
        {
            rvector.push_back(js::make_error(vctx, "Index " + std::to_string(idx) + " was < 0"));
        }

    }

    if(is_arr)
        return js::make_value(vctx, rvector);

    if(!is_arr && rvector.size() == 0)
        return rvector[0];

    return js::make_error(vctx, "Implementation error in item.load");
}

js::value item__unload(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    auto [is_arr, indices] = check_get_index_property(arg);

    if(indices.size() == 0)
        return js::make_error(vctx, "Index must be number or array of numbers, eg idx:0 or idx:[1, 2, 3]");

    int node_idx = arg.has("node") ? arg["node"] : -1;

    std::optional<std::pair<user, user_nodes>> user_and_node_opt = get_user_and_nodes(get_caller(vctx), get_thread_id(vctx));

    std::string node_name = arg["node"];

    if(!user_and_node_opt.has_value())
        return js::make_error(vctx, "User does not exist, catastrophic error");

    if(node_name != "")
    {
        for(int i=0; i < user_node_info::TYPE_COUNT; i++)
        {
            if(stolower(user_node_info::short_name[i]) == stolower(node_name))
            {
                node_idx = i;
                break;
            }
        }
    }

    std::vector<js::value> rvector;

    for(int& idx : indices)
    {
        if(idx >= 0)
        {
            std::string accum;

            auto ret = load_item_raw(node_idx, -1, idx, user_and_node_opt->first, user_and_node_opt->second, accum, get_thread_id(vctx));

            if(ret != "")
                rvector.push_back(js::make_error(vctx, ret + " for index " + std::to_string(idx)));
            else
                rvector.push_back(js::make_success(vctx));
        }
        else
        {
            rvector.push_back(js::make_error(vctx, "Index " + std::to_string(idx) + " was < 0"));
        }
    }

    if(is_arr)
        return js::make_value(vctx, rvector);

    if(!is_arr && rvector.size() == 0)
        return rvector[0];

    return js::make_error(vctx, "Implementation error in item.unload");
}

js::value push_xfer_item_id_with_logs(js::value_context& vctx, std::string item_id, user& from_user, user& to_user, bool is_pvp)
{
    if(from_user.name == to_user.name)
        return js::make_success(vctx);

    float items_to_destroy_link = 100;

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    #ifdef XFER_PATHS
    std::vector<std::string> path = playspace_network_manage.get_accessible_path_to(vctx, to_user.name, from_user.name, (path_info::path_info)(path_info::NONE | path_info::ALLOW_WARP_BOUNDARY | path_info::TEST_ACTION_THROUGH_WARP_NPCS), -1, 1.f / items_to_destroy_link);

    if(path.size() == 0)
        return js::make_error(vctx, "No path to user through the network");
    #endif // XFER_PATHS

    std::string found_item_description;

    {
        item it;

        mongo_nolock_proxy mongo_context = get_global_mongo_user_items_context(-2);

        if(!db_disk_load(mongo_context, it, item_id))
            return js::make_error(vctx, "No such item");

        found_item_description = it.get_prop("short_name") + "/" + item_id;
    }

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    auto sys_from_opt = low_level_structure_manage.get_system_of(from_user);
    auto sys_to_opt = low_level_structure_manage.get_system_of(to_user);

    if(!sys_from_opt.has_value() || !sys_to_opt.has_value())
        return js::make_error(vctx, "Really bad error: not in a system in push_xfer_item_id_with_logs");

    low_level_structure& sys_from = *sys_from_opt.value();
    low_level_structure& sys_to = *sys_to_opt.value();

    size_t current_time = get_wall_time();

    #ifdef SECLEVEL_FUNCTIONS
    if(is_pvp)
    {
        //std::cout << "max steal " << from_user.get_max_stealable_items(current_time, sys_from) << std::endl;

        //std::cout << from_user.user_limits[user_limit::ITEM_STEAL].calculate_current_data(current_time) << std::endl;
        //std::cout << "max " << from_user.get_max_stealable_items(current_time, sys_from) << std::endl;

        if(from_user.get_max_stealable_items(current_time, sys_from) < 1)
        {
            return js::make_error(vctx, "Cannot steal item currently due to seclevel limits. Please wait");
        }
        else
        {
            from_user.deplete_max_stealable_items(1, current_time, sys_from);
        }
    }
    else
    {
        if(from_user.get_max_sendable_items(current_time, sys_from, sys_to) < 1)
        {
            return js::make_error(vctx, "Cannot send item currently due to seclevel limits. Please wait");
        }
        else
        {
            from_user.deplete_max_sendable_items(1, current_time, sys_from, sys_to);
        }
    }

    #endif // SECLEVEL_FUNCTIONS

    item placeholder;

    if(placeholder.transfer_from_to_by_index(from_user.item_to_index(item_id), from_user, to_user, get_thread_id(vctx)))
    {
        user_log next;
        next.add("type", "item_xfer", "N");

        #ifdef XFER_PATHS
        playspace_network_manage.modify_path_per_link_strength_with_logs(path, -1.f / items_to_destroy_link, {next}, get_thread_id(vctx));
        #endif // XFER_PATHS

        //std::string xfer = "`NItem xfer` | from: " + from  + ", to: " + to + ", name: " + found_item_description;

        next.add("from", from_user.name, string_to_colour(from_user.name));
        next.add("to", to_user.name, string_to_colour(to_user.name));
        next.add("name", found_item_description, "");

        make_logs_on(vctx, from_user.name, user_node_info::ITEM_SEG, {next});
        make_logs_on(vctx, to_user.name, user_node_info::ITEM_SEG, {next});

        //duk_push_int(ctx, placeholder.get_prop_as_integer("item_id"));

        create_xfer_item_notif(vctx, from_user.name, to_user.name, placeholder.get_prop("short_name"));

        return js::make_success(vctx);
    }
    else
        return js::make_error(vctx, "Something went wrong (I appreciate this is not helpful)");
}

js::value push_xfer_item_with_logs(js::value_context& vctx, int item_idx, user& from, user& to, bool is_pvp)
{
    return push_xfer_item_id_with_logs(vctx, from.index_to_item(item_idx), from, to, is_pvp);
}

js::value item__xfer_to(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    auto [is_arr, indices] = check_get_index_property(arg);

    if(indices.size() == 0)
        return js::make_error(vctx, "Index must be number or array of numbers, eg idx:0 or idx:[1, 2, 3]");

    std::string from = get_caller(vctx);
    std::string to = arg["user"];

    std::optional user_and_nodes = get_user_and_nodes(get_caller(vctx), get_thread_id(vctx));

    if(!user_and_nodes.has_value())
        return js::make_error(vctx, "No such user/really catastrophic error");

    std::optional user_and_nodes_to = get_user_and_nodes(to, get_thread_id(vctx));

    if(!user_and_nodes_to)
        return js::make_error(vctx, "Destination user does not exist");

    user& usr_from = user_and_nodes->first;
    user& usr_to = user_and_nodes_to->first;

    std::vector<js::value> rvector;

    std::sort(indices.begin(), indices.end(), [](const auto& i1, const auto& i2){return i1 > i2;});

    for(auto& item_idx : indices)
    {
        std::string accum;

        auto ret = load_item_raw(-1, -1, item_idx, user_and_nodes->first, user_and_nodes->second, accum, get_thread_id(vctx));

        if(ret != "")
            rvector.push_back(js::make_error(vctx, ret + " for index " + std::to_string(item_idx)));
        else
            rvector.push_back(push_xfer_item_with_logs(vctx, item_idx, usr_from, usr_to, false));
    }

    {
        mongo_lock_proxy mctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        usr_from.overwrite_user_in_db(mctx);
        usr_to.overwrite_user_in_db(mctx);
    }

    if(is_arr)
        return js::make_value(vctx, rvector);

    if(!is_arr && rvector.size() == 0)
        return rvector[0];

    return js::make_error(vctx, "Implementation error in item.xfer_to");
}

js::value item__bundle_script(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int item_idx = arg.has("idx") ? arg["idx"] : -1;
    std::string scriptname = arg["name"];
    std::string tag = arg["tag"];

    if(tag.size() > 8)
        return js::make_error(vctx, "Tag must be <= 8 characters");

    if(scriptname == "")
        return js::make_error(vctx, "Invalid name");

    std::string full_script_name = get_caller(vctx) + "." + scriptname;

    if(!is_valid_full_name_string(full_script_name))
        return js::make_error(vctx, "Invalid name");

    if(item_idx < 0)
        return js::make_error(vctx, "Invalid index");

    user current_user;

    {
        mongo_nolock_proxy user_lock = get_global_mongo_user_info_context(get_thread_id(vctx));

        current_user.load_from_db(user_lock, get_caller(vctx));
    }

    std::string item_id = current_user.index_to_item(item_idx);

    if(item_id == "")
        return js::make_error(vctx, "Invalid index");

    {
        mongo_lock_proxy item_lock = get_global_mongo_user_items_context(get_thread_id(vctx));

        item found_bundle;
        found_bundle.item_id = item_id;

        if(!db_disk_exists(item_lock, found_bundle))
            return js::make_error(vctx, "No such item");

        db_disk_load(item_lock, found_bundle, item_id);

        if((int)found_bundle.get("item_type") != item_types::EMPTY_SCRIPT_BUNDLE)
            return js::make_error(vctx, "Not a script bundle");

        if((int)found_bundle.get("full") != 0)
            return js::make_error(vctx, "Not an empty script bundle");

        /*item found_script;

        if(!found_bundle.exists_in_db(item_lock, full_script_name))
            return push_error(ctx, "No such script");

        found_script.load_from_db(item_lock, full_script_name);*/

        script_info found_script;
        found_script.name = full_script_name;

        if(!found_script.load_from_db(item_lock))
            return js::make_error(vctx, "No such script or invalid script");

        if(!found_script.valid)
            return js::make_error(vctx, "Script invalid");

        int max_storage = found_bundle.get("max_script_size");

        if((int)found_script.unparsed_source.size() > max_storage)
            return js::make_error(vctx, "Empty bundle does not contain enough space");


        std::string name = found_bundle.get_prop("short_name");

        if(tag != "")
        {
            name += " [" + tag + "]";

            found_bundle.set_as("short_name", name);
        }

        found_script.fill_as_bundle_compatible_item(found_bundle);
        found_bundle.set_as("full", 1);

        db_disk_overwrite(item_lock, found_bundle);
    }

    return js::make_success(vctx);
}


js::value item__register_bundle(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int item_idx = arg.has("idx") ? arg["idx"] : -1;
    std::string scriptname = arg["name"];

    if(scriptname == "")
        return js::make_error(vctx, "Invalid name");

    std::string full_script_name = get_caller(vctx) + "." + scriptname;

    if(!is_valid_full_name_string(full_script_name))
        return js::make_error(vctx, "Invalid name");

    if(item_idx < 0)
        return js::make_error(vctx, "Invalid index");

    user current_user;

    {
        mongo_nolock_proxy user_lock = get_global_mongo_user_info_context(get_thread_id(vctx));

        current_user.load_from_db(user_lock, get_caller(vctx));
    }

    std::string item_id = current_user.index_to_item(item_idx);

    if(item_id == "")
        return js::make_error(vctx, "Invalid index");

    if(!current_user.has_loaded_item(item_id))
        return js::make_error(vctx, "Item not loaded");

    {
        mongo_lock_proxy item_lock = get_global_mongo_user_items_context(get_thread_id(vctx));

        item found_bundle;
        found_bundle.item_id = item_id;

        if(!db_disk_exists(item_lock, found_bundle))
            return js::make_error(vctx, "No such item");

        db_disk_load(item_lock, found_bundle, item_id);

        if((int)found_bundle.get("item_type") != item_types::EMPTY_SCRIPT_BUNDLE)
            return js::make_error(vctx, "Not a script bundle");

        if((int)found_bundle.get("full") != 1)
            return js::make_error(vctx, "Not a full script bundle");

        found_bundle.set_as("registered_as", scriptname);

        db_disk_overwrite(item_lock, found_bundle);
    }

    return js::make_success(vctx);
}

std::string on_breach_name_to_real_script_name(std::string script_name, const std::string& host)
{
    if(script_name.find('.') == std::string::npos && script_name.size() > 0)
        script_name = host + "." + script_name;

    if(script_name.size() == 0)
        script_name = host + ".on_breach";

    if(!is_valid_full_name_string(script_name))
        return "";

    return script_name;
}

js::value item__configure_on_breach(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int item_idx = arg.has("idx") ? arg["idx"] : -1;
    std::string scriptname = arg["name"];

    if(scriptname.size() > 60)
        return js::make_error(vctx, "Too long script name");

    bool has_name = arg["name"].is_truthy();
    bool confirm = arg["confirm"].is_truthy();

    if(item_idx == -1)
        return js::make_error(vctx, "Usage: idx:num, name:\"script_name\"");

    user usr;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!usr.load_from_db(mongo_ctx, get_caller(vctx)))
            return js::make_error(vctx, "Invalid calling user");
    }

    std::string item_id = usr.index_to_item(item_idx);

    if(item_id == "")
        return js::make_error(vctx, "Invalid item");

    item it;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

        if(!db_disk_load(mongo_ctx, it, item_id))
            return js::make_error(vctx, "No such item");
    }

    if((int)it.get("item_type") != item_types::ON_BREACH)
        return js::make_error(vctx, "Wrong item type, must be on_breach");

    std::string new_name = on_breach_name_to_real_script_name(scriptname, priv_ctx.original_host);
    std::string real_name = on_breach_name_to_real_script_name(it.get_prop("script_name"), priv_ctx.original_host);

    if(!has_name)
        return js::make_success(vctx);

    if(new_name == "")
        return js::make_error(vctx, "Invalid name");

    if(has_name && !confirm)
    {
        //?
        return js::make_error(vctx, "Please run " + make_key_col("confirm") + ":" + make_val_col("true") + " to confirm setting on_breach script name to " + new_name);
    }

    if(has_name && confirm)
    {
        it.set_as("script_name", scriptname);

        {
             mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

             db_disk_overwrite(mongo_ctx, it);
        }

        return js::make_success(vctx);
    }

    return js::make_error(vctx, "Should be impossible to reach here");
}

#if defined(TESTING) || defined(EXTRAS)
duk_ret_t item__create(priv_context& priv_ctx, duk_context* ctx, int sl)
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

    std::string short_name = duk_safe_get_prop_string(ctx, -1, "short_name");
    std::string description = duk_safe_get_prop_string(ctx, -1, "description");

    if(short_name == "" && description == "")
        test_item = item_types::get_default_of((item_types::item_type)item_type, lock_type);
    else
        test_item = item_types::get_named_describer(short_name, description);

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
        db_disk_overwrite(mongo_ctx, test_item);
    }

    if(test_item.transfer_to_user(get_caller(ctx), get_thread_id(ctx)))
        duk_push_int(ctx, (int)test_item.get("item_id"));
    else
        push_error(ctx, "Could not transfer item to caller");

    return 1;
}
#endif // TESTING

///modify to return a string unfortunately
///breaking api change
js::value cash__expose(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string from = arg["user"];
    bool arr = requested_scripting_api(arg);

    if(from == "")
        return js::make_error(vctx, "Args: user:<username>");

    auto opt_user_and_nodes = get_user_and_nodes(from, get_thread_id(vctx));

    if(!opt_user_and_nodes.has_value())
        return js::make_error(vctx, "No such user");

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    if(!low_level_structure_manage.in_same_system(get_caller(vctx), from))
        return js::make_error(vctx, "Must be in same system to expose cash");

    auto hostile = opt_user_and_nodes->second.valid_hostile_actions();

    size_t current_time = get_wall_time();

    if((hostile & user_node_info::XFER_CASH_FROM) > 0)
    {
        user& usr = opt_user_and_nodes->first;

        auto sys_opt = low_level_structure_manage.get_system_of(usr);

        if(!sys_opt.has_value())
            return js::make_error(vctx, "Error in cash expose (system)");

        low_level_structure& sys = *sys_opt.value();

        if(!arr)
        {
            std::string str =
            "exposed: " + to_string_with_enforced_variable_dp(usr.cash, 2) + "\n" +
            "available: " + to_string_with_enforced_variable_dp(usr.get_max_stealable_cash(current_time, sys), 2);

            return js::make_value(vctx, str);
        }
        else
        {
            nlohmann::json ret;
            ret["exposed"] = usr.cash;
            ret["available"] = usr.get_max_stealable_cash(current_time, sys);

            return js::make_value(vctx, ret);
        }
    }
    else
    {
        return js::make_error(vctx, "System Cash Node Secured");
    }
}


js::value item__expose(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    int pretty = !requested_scripting_api(arg);
    int full = arg["full"];

    std::string from = arg["user"];

    if(from == "")
        return js::make_error(vctx, "Args: user:<username>");

    auto opt_user_and_nodes = get_user_and_nodes(from, get_thread_id(vctx));

    if(!opt_user_and_nodes.has_value())
        return js::make_error(vctx, "No such user");

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    if(!low_level_structure_manage.in_same_system(get_caller(vctx), from))
        return js::make_error(vctx, "Must be in same system to expose items");

    //user& usr = opt_user_and_nodes->first;

    //printf("%i num\n", usr.upgr_idx.size());
    //std::cout << "name " << usr.name << std::endl;

    auto hostile = opt_user_and_nodes->second.valid_hostile_actions();

    if((hostile & user_node_info::XFER_ITEM_FROM) > 0)
    {
        return push_internal_items_view(vctx, pretty, full, opt_user_and_nodes->second, opt_user_and_nodes->first, "", true);
    }
    else
    {
        return js::make_error(vctx, "System Item Node Secured");
    }
}


std::optional<js::value> handle_confirmed(js::value_context& vctx, bool confirm, const std::string& username, double price)
{
    std::optional opt_user = get_user(username, get_thread_id(vctx));

    if(!opt_user.has_value())
        return js::make_error(vctx, "No such user");

    if(isnan(price))
        return js::make_error(vctx, "NaN");

    int iprice = price;

    if(!confirm)
        return js::make_error(vctx, "Please confirm:true to pay " + std::to_string(iprice));

    if(opt_user->cash < iprice)
        return js::make_error(vctx, "Please acquire more wealth");

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));
        opt_user->cash -= iprice;

        user_log next;
        next.add("type", "cash_xfer", "X");
        next.add("from", username, string_to_colour(username));
        next.add("to", "core", string_to_colour("core"));
        next.add("amount", std::to_string(price), "");

        make_logs_on(vctx, username, user_node_info::CASH_SEG, {next});

        create_xfer_notif(vctx, username, "", price);

        opt_user->overwrite_user_in_db(mongo_ctx);
    }

    return std::nullopt;
}

duk_ret_t take_cash(duk_context* ctx, const std::string& username, double price)
{
    std::optional opt_user = get_user(username, get_thread_id(ctx));

    if(!opt_user.has_value())
        return push_error(ctx, "No such user");

    if(opt_user->cash < price)
        return push_error(ctx, "Please acquire more wealth");

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));
        opt_user->cash -= price;

        opt_user->overwrite_user_in_db(mongo_ctx);
    }

    return 0;
}

///have item__steal reset internal node structure

js::value item__steal(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string from = arg["user"];

    auto [is_arr, indices] = check_get_index_property(arg);

    if(indices.size() == 0)
        return js::make_error(vctx, "Pass idx:item_offset or idx:[offset1, offset2]");

    if(from == "")
        return js::make_error(vctx, "Args: user:<username>, idx:item_offset");

    auto found = get_user_and_nodes(from, get_thread_id(vctx));

    if(!found.has_value())
        return js::make_error(vctx, "Error or no such user");

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    if(!low_level_structure_manage.in_same_system(get_caller(vctx), from))
        return js::make_error(vctx, "Must be in same system to steal items");

    //#define STEALDEBUG
    #ifdef STEALDEBUG
    ///for some reason even though the limit is 3, we could only steal 2
    low_level_structure& sys_1 = *low_level_structure_manage.get_system_of(from).value();

    std::cout << sys_1.get_ratelimit_max_item_steal() << std::endl;
    #endif // STEALDEBUG

    user& found_user = found->first;
    user_nodes& nodes = found->second;

    auto hostile = nodes.valid_hostile_actions();

    if(!((hostile & user_node_info::XFER_ITEM_FROM) > 0))
        return js::make_error(vctx, "System Item Node Secured");

    size_t current_time = get_wall_time();

    auto sys_from_opt = low_level_structure_manage.get_system_of(from);

    if(!sys_from_opt.has_value())
        return js::make_error(vctx, "Catastrophic error, lost in item.steal");

    #ifdef SECLEVEL_FUNCTIONS

    low_level_structure& sys_from = *low_level_structure_manage.get_system_of(from).value();

    int max_stealable = found_user.get_max_stealable_items(current_time, sys_from);

    if(max_stealable < (int)indices.size())
    {
        return js::make_error(vctx, "Cannot steal due to seclevel limits, check #item.expose({user:\"" + from + "\"})");
    }

    #endif // SECLEVEL_FUNCTIONS

    //std::string item_id = found_user.index_to_item(item_idx);
    int cost = 0;
    bool loaded_lock = false;

    std::vector<std::string> item_ids;

    for(int i=0; i < (int)indices.size(); i++)
    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

        std::string item_id = found_user.index_to_item(indices[i]);

        item it;

        if(!db_disk_load(mongo_ctx, it, item_id))
            continue;

        if((int)it.get("item_type") == item_types::LOCK && nodes.any_contains_lock(item_id))
        {
            if(loaded_lock)
                return js::make_error(vctx, "Cannot steal more than one loaded lock");

            cost += 50;
            loaded_lock = true;
        }
        else
        {
            cost += 20;
        }

        item_ids.push_back(item_id);
    }

    bool confirm = arg["confirm"].is_truthy();

    if(auto val = handle_confirmed(vctx, confirm, get_caller(vctx), cost); val.has_value())
        return val.value();

    if(loaded_lock)
    {
        nodes.reset_all_breach();

        for(auto& i : nodes.nodes)
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

            std::vector<item> all_locks = i.get_locks(mongo_ctx);

            for(item& it : all_locks)
            {
                it.force_rotate();

                db_disk_overwrite(mongo_ctx, it);
            }
        }

        {
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));
            nodes.overwrite_in_db(node_ctx);
        }
    }

    std::vector<js::value> rvector;

    mongo_lock_proxy steal_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

    user to_user;
    to_user.load_from_db(steal_ctx, get_caller(vctx));

    for(auto& item_id : item_ids)
    {
        std::string accum;
        auto ret = load_item_raw(-1, -1, found_user.item_to_index(item_id), found_user, nodes, accum, get_thread_id(vctx));

        if(ret != "")
            rvector.push_back(js::make_error(vctx, ret));
        else
            rvector.push_back(push_xfer_item_id_with_logs(vctx, item_id, found_user, to_user, true));
    }

    to_user.overwrite_user_in_db(steal_ctx);
    found_user.overwrite_user_in_db(steal_ctx);

    if(is_arr)
        return js::make_value(vctx, rvector);

    if(!is_arr && rvector.size() == 0)
        return rvector[0];

    return js::make_error(vctx, "Implementation error in item.steal");
}

js::value cash__steal(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string from = arg["user"];

    if(from == "")
        return js::make_error(vctx, "Args: user:<username>, amount:<number>");

    double amount = arg["amount"];

    if(amount == 0)
        return js::make_error(vctx, "amount is not a number, or 0");

    user target;

    {
        mongo_nolock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!target.load_from_db(user_ctx, from))
            return js::make_error(vctx, "Target does not exist");
    }

    user_nodes nodes;

    {
        mongo_nolock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));

        nodes.ensure_exists(node_ctx, from);
        nodes.load_from_db(node_ctx, from);
    }

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    if(!low_level_structure_manage.in_same_system(get_caller(vctx), from))
        return js::make_error(vctx, "Must be in same system to steal cash");

    auto hostile = nodes.valid_hostile_actions();

    if((hostile & user_node_info::XFER_CASH_FROM) > 0)
    {
        return cash_internal_xfer(vctx, from, get_caller(vctx), amount, true);
    }
    else
    {
        return js::make_error(vctx, "System Cash Node Secured");
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

duk_ret_t user__port(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = get_host_from_fullname(priv_ctx.called_as);

    //std::cout << "user_name " << name_of_person_being_attacked << std::endl;

    user usr;

    {
        mongo_lock_proxy user_info = get_global_mongo_user_info_context(get_thread_id(ctx));
        user_info.change_collection(name_of_person_being_attacked);

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


js::value nodes__view_log(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string name_of_person_being_attacked = arg["user"];

    int make_array = requested_scripting_api(arg);

    user usr;

    {
        mongo_nolock_proxy user_info = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!usr.load_from_db(user_info, name_of_person_being_attacked))
            return js::make_error(vctx, "No such user");
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));

        nodes.ensure_exists(node_ctx, name_of_person_being_attacked);
        nodes.load_from_db(node_ctx, name_of_person_being_attacked);
    }

    std::string node_fullname = arg["NID"];

    std::vector<item> attackables;

    user_node* current_node = nullptr;

    {
        current_node = nodes.name_to_node(name_of_person_being_attacked + "_" + node_fullname);

        if(current_node == nullptr)
        {
            throw std::runtime_error("Node nullptr, impossible " + name_of_person_being_attacked);
        }

        {
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

            attackables = current_node->get_locks(item_ctx);
        }
    }

    if(current_node == nullptr)
        return js::make_error(vctx, "Misc error: Blue Melon");

    if(name_of_person_being_attacked != get_caller(vctx))
    {
        ///there must be both an accessible path, and the node itself must be breached
        if(!nodes.node_accessible(*current_node) || !current_node->is_breached())
        {
            return js::make_value(vctx, nodes.get_lockdown_message());
        }
    }

    auto logs = current_node->new_logs;

    std::vector<nlohmann::json> all_logs;

    for(user_log& i : logs)
    {
        all_logs.push_back(i.script_fetch());
    }

    if(make_array)
    {
       return js::make_value(vctx, all_logs);
    }
    else
    {
        std::string str = format_pretty_names(logs, false);

        return js::make_value(vctx, str);
    }
}

js::value log__expose(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    return nodes__view_log(priv_ctx, vctx, arg, sl);
}

js::value hack_internal(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, const std::string& name_of_person_being_attacked, bool is_arr)
{
    auto user_and_nodes = get_user_and_nodes(name_of_person_being_attacked, get_thread_id(vctx));

    if(!user_and_nodes.has_value())
        return js::make_error(vctx, "No such user");

    user& usr = user_and_nodes->first;
    user_nodes& nodes = user_and_nodes->second;

    std::string node_fullname = arg["NID"];

    std::vector<item> attackables;

    user_node* current_node = nullptr;

    //if(node_fullname == "")
    {
        //current_node = nodes.get_front_node();

        current_node = nodes.name_to_node(name_of_person_being_attacked + "_" + node_fullname);

        {
            mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

            attackables = current_node->get_locks(item_ctx);
        }
    }

    if(current_node == nullptr)
        return js::make_error(vctx, "Misc error: Black Tiger");

    nlohmann::json targeted;
    targeted["NID_string"] = current_node->get_NID();
    targeted["short_name"] = current_node->get_short_name();
    targeted["name"] = current_node->get_long_name();

    nlohmann::json array_data;

    array_data["target"] = targeted;
    array_data["locked"] = false;
    array_data["accessible"] = true;

    if(!nodes.node_accessible(*current_node))
    {
        std::string msg = nodes.get_lockdown_message();

        array_data["ok"] = false;
        array_data["locked"] = true;
        array_data["msg"] = msg;
        array_data["accessible"] = false;

        if(!is_arr)
        {
            return js::make_value(vctx, msg);
        }
        else
        {
           return js::make_value(vctx, array_data);
        }
    }

    ///leave trace in logs
    {
        user attacker;

        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

            attacker.load_from_db(mongo_ctx, get_caller(vctx));
        }

        nodes.leave_trace(*current_node, attacker.name, usr, get_thread_id(vctx));

        ///hmm, we are actually double overwriting here
        {
            mongo_nolock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));

            nodes.overwrite_in_db(node_ctx);
        }
    }

    ///if(current_node.breached)
    ///do display adjacents, node type, what we can do here

    bool all_success = true;

    std::string msg;

    array_data["breached"] = current_node->is_breached();

    if(!current_node->is_breached())
    {
        for(item& i : attackables)
        {
            if(i.should_rotate())
            {
                i.handle_rotate();

                ///synchronous so that multiple things don't rotate
                mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));
                db_disk_overwrite(item_ctx, i);

                ///todo: send a chats.tell to victim here
            }

            if(i.is_breached())
                continue;

            std::string func = i.get_prop("lock_type");

            auto it = secret_map.find(func);

            ///is a lock
            if(it != secret_map.end())
            {
                array_data["lock_type"] = func;

                if(!it->second.ptr(priv_ctx, vctx, arg, msg, i, name_of_person_being_attacked))
                {
                    all_success = false;

                    array_data["locked"] = true;

                    break;
                }
                else
                {
                    ///todo: send a chats.tell to victim here
                    i.breach();

                    array_data["lock_breached"] = true;

                    create_notification(get_thread_id(vctx), name_of_person_being_attacked, make_notif_col("-" + i.get_prop("short_name") + " breached-"));

                    ///wants to be synchronous so that we don't overlap writes
                    mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));
                    db_disk_overwrite(item_ctx, i);
                }
            }
        }
    }

    if(msg.size() > 0)
        array_data["lock_msg"] = msg;

    user_node* breach_node = nodes.type_to_node(user_node_info::BREACH);
    user_node* front_node = nodes.type_to_node(user_node_info::FRONT);

    if(breach_node == nullptr)
        return js::make_error(vctx, "Error Code: Yellow Panther in hack_internal (net.hack?)");

    bool breach_is_breached = breach_node->is_breached();

    if(current_node->is_breached())
    {
        std::string dat = current_node->get_breach_message(usr, nodes);

        msg += dat;
    }

    ///do info here first, then display the breach message the next time round
    finalise_info(msg, all_success, current_node->is_breached(), attackables.size());

    if(all_success && !current_node->is_breached())
    {
        std::string dat = current_node->get_breach_message(usr, nodes);

        msg += dat;

        create_notification(get_thread_id(vctx), name_of_person_being_attacked, make_error_col("-" + user_node_info::long_names[current_node->type] + " Node Compromised-"));
    }

    if(all_success)
    {
        current_node->breach();

        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));

        nodes.overwrite_in_db(node_ctx);
    }

    if(current_node->is_breached())
    {
        array_data["connected"] = current_node->get_breach_json(usr, nodes);
        array_data["locked"] = false;
    }
    else
    {
        array_data["locked"] = true;
    }

    array_data["breached"] = current_node->is_breached();

    if(front_node && front_node->is_breached())
    {
        quest_manager& qm = get_global_quest_manager();

        quest_hack_data br;
        br.target = name_of_person_being_attacked;

        qm.process(get_thread_id(vctx), get_caller(vctx), br);
    }

    if(breach_node->is_breached())
    {
        quest_manager& qm = get_global_quest_manager();

        quest_breach_data br;
        br.target = name_of_person_being_attacked;

        qm.process(get_thread_id(vctx), get_caller(vctx), br);
    }

    if(breach_node->is_breached() && !breach_is_breached)
    {
        std::vector<item> all_items;

        {
            mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

            all_items = usr.get_all_items(item_ctx);
        }

        for(item& it : all_items)
        {
            if((int)it.get("item_type") == item_types::ON_BREACH && usr.has_loaded_item(it.item_id))
            {
                std::string script_name = it.get_prop("script_name");

                script_name = on_breach_name_to_real_script_name(script_name, usr.name);

                if(script_name == "")
                    continue;

                script_name = "#" + script_name + "({attacker:\"" + get_caller(vctx) + "\"})";

                ///remember that this includes user.call_stack weirdness
                ///500ms exec time
                throwaway_user_thread(usr.name, script_name, 500. / 1000., true);

                //std::cout << "res " << run_in_user_context(usr.name, script_name, std::nullopt, 500. / 1000., true);

                break;
            }
        }
        //std::cout << "srun\n";
    }

    while(msg.size() > 0 && msg.back() == '\n')
        msg.pop_back();

    array_data["msg"] = msg;
    array_data["ok"] = true;

    if(!is_arr)
        return js::make_value(vctx, msg);
    else
        return js::make_value(vctx, array_data);
}

#ifdef USE_LOCS

duk_ret_t user__port(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = get_host_from_fullname(priv_ctx.called_as);

    return hack_internal(priv_ctx, ctx, name_of_person_being_attacked);
}
#endif // USE_LOCS


js::value net__hack(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    #ifdef TESTING
    /*MAKE_PERF_COUNTER();
    mongo_diagnostics diagnostic_scope;*/
    #endif // TESTING

    std::string name_of_person_being_attacked = arg["user"];
    bool is_arr = requested_scripting_api(arg);

    if(name_of_person_being_attacked == "")
        return js::make_error(vctx, "Usage: net.hack({user:<name>})");

    if(!get_user(name_of_person_being_attacked, get_thread_id(vctx)))
        return js::make_error(vctx, "No such user");

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_npc_properties_context(get_thread_id(vctx));

        if(npc_info::has_type(mongo_ctx, npc_info::WARPY, name_of_person_being_attacked))
        {
            return js::make_value(vctx, make_error_col("-Access Denied-"));
        }
    }

    bool cheats = false;

    /*#ifdef TESTING
    cheats = true;
    #endif // TESTING*/

    if(!cheats)
    {
        playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

        float hack_cost = 0.25f;

        auto path = playspace_network_manage.get_accessible_path_to(vctx, name_of_person_being_attacked, get_caller(vctx), (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS), -1, hack_cost);

        if(path.size() == 0)
            return js::make_error(vctx, "No Path");

        user_log next;
        next.add("type", "hostile_path_access", "");

        playspace_network_manage.modify_path_per_link_strength_with_logs(path, -hack_cost, {next}, get_thread_id(vctx));
    }

    return hack_internal(priv_ctx, vctx, arg, name_of_person_being_attacked, is_arr);
}

///ok so new hacking
///takes a certain amount of time
///therefore we need some sort of longer running process to handle it, akin to realtime scripting
///but we also want people to be able to run commands during it
///maybe i can create a server realtime script that executes, would use relatively low cpu,
///although might require the advanced one thread <-> many thread watcher vs exec model for the server's sanity
#if 0
duk_ret_t net__hack_new(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    #ifdef TESTING
    /*MAKE_PERF_COUNTER();
    mongo_diagnostics diagnostic_scope;*/
    #endif // TESTING

    std::string name_of_person_being_attacked = duk_safe_get_prop_string(ctx, -1, "user");
    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");

    if(name_of_person_being_attacked == "")
        return push_error(ctx, "Usage: net.hack({user:<name>})");

    if(!get_user(name_of_person_being_attacked, get_thread_id(ctx)))
        return push_error(ctx, "No such user");

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_npc_properties_context(get_thread_id(ctx));

        if(npc_info::has_type(mongo_ctx, npc_info::WARPY, name_of_person_being_attacked))
        {
            push_duk_val(ctx, make_error_col("-Access Denied-"));
            return 1;
        }
    }

    bool cheats = false;

    /*#ifdef TESTING
    cheats = true;
    #endif // TESTING*/

    if(!cheats)
    {
        playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

        float hack_cost = 0.25f;

        js::value_context vctx(ctx);

        auto path = playspace_network_manage.get_accessible_path_to(vctx, name_of_person_being_attacked, get_caller(ctx), (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS), -1, hack_cost);

        if(path.size() == 0)
            return push_error(ctx, "No Path");

        user_log next;
        next.add("type", "hostile_path_access", "");

        playspace_network_manage.modify_path_per_link_strength_with_logs(path, -hack_cost, {next}, get_thread_id(ctx));
    }

    return push_error(ctx, "unimplemented");
}
#endif

js::value nodes__manage(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    bool get_array = requested_scripting_api(arg);

    std::string usage = "Usage: " + make_key_val("swap", "[idx1, idx2]");

    ///reorder
    bool has_swap = arg["swap"].is_truthy();

    user usr;

    {
        mongo_nolock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!usr.load_from_db(user_ctx, get_caller(vctx)))
            return js::make_error(vctx, "No such user, really bad error");
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));

        ///yeah this isn't good enough, need to do what we did for locs?
        ///or just do it in loc handler i guess
        nodes.ensure_exists(node_ctx, get_caller(vctx));

        nodes.load_from_db(node_ctx, get_caller(vctx));
    }

    if(has_swap)
    {
        if(sl > 1)
            return js::make_error(vctx, "Must be called with a sec level of 1 to swap");

        std::vector<int> vals = arg["swap"];

        if(vals.size() != 2)
            return js::make_error(vctx, "array len != 2");

        std::vector<std::string> items;

        for(auto& i : vals)
        {
            std::string item = usr.index_to_item(i);

            if(item == "")
                return js::make_error(vctx, "Item does not exist");

            items.push_back(item);
        }

        auto u1 = nodes.lock_to_node(items[0]);
        auto u2 = nodes.lock_to_node(items[1]);

        if(!u1.has_value() || !u2.has_value())
            return js::make_error(vctx, "Item not found on node");

        auto p1 = u1.value()->lock_to_pointer(items[0]);
        auto p2 = u2.value()->lock_to_pointer(items[1]);

        if(!p1.has_value() || !p2.has_value())
            return js::make_error(vctx, "Item not found on node");

        ///can cause multiples to be loaded on one stack
        std::swap(*p1.value(), *p2.value());

        /*{
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

            u1.value()->breach();
            u2.value()->breach();

            nodes.overwrite_in_db(node_ctx);
        }*/

        {
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(vctx));
            nodes.overwrite_in_db(node_ctx);
        }

        {
            mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

            item i1, i2;
            db_disk_load(items_ctx, i1, items[0]);
            db_disk_load(items_ctx, i2, items[1]);

            i1.breach();
            i2.breach();

            db_disk_overwrite(items_ctx, i1);
            db_disk_overwrite(items_ctx, i2);
        }

        return js::make_success(vctx);
    }


    if(!get_array)
    {
        std::string accum = usage + "\n" + "Node Key: ";

        for(int i=0; i < (int)user_node_info::TYPE_COUNT; i++)
        {
            accum += user_node_info::short_name[i] + ": " + user_node_info::long_names[i];

            user_node* node = nodes.type_to_node((user_node_t)i);

            if(node != nullptr)
            {
                accum += " (" + node->get_NID() + ")";
            }

            if(i != user_node_info::TYPE_COUNT-1)
                accum += ", ";
        }

        accum += "\n";

        {
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

            ///this needs to take a user as well
            ///so that we can display the indices of the items for easy load/unload
            for(user_node& node : nodes.nodes)
            {
                accum += node.get_pretty(item_ctx, usr);
            }
        }

        if(accum.size() > 0 && accum.back() == '\n')
            accum.pop_back();

        return js::make_value(vctx, accum.c_str());
    }
    else
    {
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(vctx));

        return js::make_value(vctx, nodes.get_as_json(item_ctx, usr));
    }
}

#ifdef USE_LOCS

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
#endif // USE_LOCS

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

#if 0
duk_ret_t net__view(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string from = duk_safe_get_prop_string(ctx, -1, "user");
    int pretty = !dukx_is_prop_truthy(ctx, -1, "array");

    if(from == "")
        return push_error(ctx, "usage: net.view({user:<username>})");

    std::optional opt_user_and_nodes = get_user_and_nodes(from, get_thread_id(ctx));

    if(!opt_user_and_nodes.has_value())
        return push_error(ctx, "No such user");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(!playspace_network_manage.has_accessible_path_to(ctx, from, get_caller(ctx), path_info::VIEW_LINKS))
       return push_error(ctx, "Inaccessible");

    user& usr = opt_user_and_nodes->first;

    auto hostile_actions = opt_user_and_nodes->second.valid_hostile_actions();

    if(!usr.is_allowed_user(get_caller(ctx)) && !((hostile_actions & user_node_info::VIEW_LINKS) > 0))
        return push_error(ctx, "Node is Locked");

    std::vector<std::string> links = playspace_network_manage.get_links(from);
    std::vector<float> stabs;

    for(int i=0; i < (int)links.size(); i++)
    {
        auto val = playspace_network_manage.get_neighbour_link_strength(usr.name, links[i]);

        if(val.has_value())
            stabs.push_back(val.value());
        else
            stabs.push_back(-1.f);
    }

    if(!pretty)
    {
        std::vector<json> all;

        for(int i=0; i < (int)links.size(); i++)
        {
            json j;
            j["neighbour"] = links[i];
            j["stability"] = stabs[i];

            all.push_back(j);
        }

        json j = all;

        push_duk_val(ctx, all);
    }
    else
    {
        std::string str;

        for(int i=0; i < (int)links.size(); i++)
        {
            str += links[i] + ", " + to_string_with_enforced_variable_dp(stabs[i], 2) + "\n";
        }

        push_duk_val(ctx, str);
    }

    return 1;
}
#endif // 0


double npc_name_to_angle(const std::string& str)
{
    uint32_t val = std::hash<std::string>{}(str) % (size_t)(pow(2, 32) - 1);

    return ((double)val / (pow(2, 32)-1)) * 2 * M_PI;
}

///so
///net.map is probably going to die (sadface)
///because its no longer the main way of contenting your way around
///instead npcs will be situated in a system
///you'll get shown the door to it, but managing the complexity of the internal cloud is up to you
///make sys.map
js::value net__map(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    #ifdef OLD_DEPRECATED
    COOPERATE_KILL();

    int w = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "w", 40);
    int h = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "h", 30);

    if(w <= 5 || h <= 5)
        return push_error(ctx, "w or h <= 5");

    #define MAX_TERM 360

    if(w > MAX_TERM || h > MAX_TERM)
        return push_error(ctx, "w or h > " + std::to_string(MAX_TERM));

    std::string from = duk_safe_get_prop_string(ctx, -1, "user");
    int num = duk_get_prop_string_as_int(ctx, -1, "n", 2);

    if(from == "")
        return push_error(ctx, "usage: net.map({user:<username>, n:2})");

    if(num < 0 || num > 15)
        return push_error(ctx, "n out of range [1,15]");

    if(!get_user(from, get_thread_id(ctx)).has_value())
        return push_error(ctx, "User does not exist");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(!playspace_network_manage.has_accessible_path_to(ctx, from, get_caller(ctx), path_info::VIEW_LINKS))
        return push_error(ctx, "Target Inaccessible");

    network_accessibility_info info = playspace_network_manage.generate_network_accessibility_from(ctx, from, num);

    vec3f cur_center = info.global_pos[from];

    auto buffer = ascii_make_buffer({w, h}, false);

    std::string built = ascii_render_from_accessibility_info(info, buffer, cur_center);

    push_duk_val(ctx, built);

    return 1;
    #else
    return sys__view(priv_ctx, vctx, arg, sl);
    #endif
}

std::vector<nlohmann::json> get_net_view_data_arr(network_accessibility_info& info)
{
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    std::vector<nlohmann::json> all_npc_data;

    for(auto& i : info.ring_ordered_names)
    {
        const std::string& name = i;
        vec3f pos = info.global_pos[name];

        nlohmann::json j;
        j["name"] = name;
        j["x"] = pos.x();
        j["y"] = pos.y();
        j["z"] = pos.z();

        auto connections = playspace_network_manage.get_links(name);

        for(auto it = connections.begin(); it != connections.end();)
        {
            if(info.accessible.find(*it) == info.accessible.end())
                it = connections.erase(it);
            else
                it++;
        }

        std::vector<float> stabs;

        for(int i=0; i < (int)connections.size(); i++)
        {
            auto val = playspace_network_manage.get_neighbour_link_strength(name, connections[i]);

            if(val.has_value())
                stabs.push_back(val.value());
            else
                stabs.push_back(-1.f);
        }


        j["links"] = connections;
        j["stabilities"] = stabs;

        all_npc_data.push_back(j);
    }

    return all_npc_data;
}

std::string get_net_view_data_str(std::vector<nlohmann::json>& all_npc_data, bool include_position = true)
{
    std::string str;

    std::vector<std::string> all_names{"Name"};
    std::vector<std::string> all_positions{"Position"};
    std::vector<std::string> all_links{"Links"};

    for(nlohmann::json& j : all_npc_data)
    {
        std::string name = j["name"];
        vec3f pos = (vec3f){j["x"], j["y"], j["z"]};
        std::vector<std::string> links = j["links"];
        std::vector<float> stabs = j["stabilities"];

        std::string pos_str = std::to_string(pos.x()) + " " + std::to_string(pos.y()) + " " + std::to_string(pos.z());

        std::string link_str = "[";

        for(int i=0; i < (int)links.size(); i++)
        {
            std::string stab_str = "`c" + to_string_with_enforced_variable_dp(stabs[i], 2) + "`";

            if(i != (int)links.size()-1)
                link_str += colour_string(links[i]) + " " + stab_str + ", ";
            else
                link_str += colour_string(links[i]) + " " + stab_str;
        }

        link_str += "]";

        all_names.push_back(name);
        all_positions.push_back(pos_str);
        all_links.push_back(link_str);
    }

    for(int i=0; i < (int)all_names.size(); i++)
    {
        std::string formatted_name = format_by_vector(all_names[i], all_names);
        std::string formatted_pos = format_by_vector(all_positions[i], all_positions);
        //std::string formatted_link = format_by_vector(all_links[i], all_links);

        std::string formatted_link = all_links[i];

        std::string rname = formatted_name;

        int pnum = 0;
        while(rname.size() > 0 && rname.back() == ' ')
        {
            rname.pop_back();
            pnum++;
        }

        if(i != 0)
        {
            rname = colour_string(rname);

            for(int kk=0; kk < pnum; kk++)
            {
                rname += " ";
            }

            formatted_name = rname;
        }

        if(include_position)
            str += formatted_name + " | " + formatted_pos + " | " + formatted_link + "\n";
        else
            str += formatted_name + " | " + formatted_link + "\n";
    }

    ///only one link
    if(all_names.size() == 2 && !include_position)
    {
        return all_links[1];
    }

    return str;
}

js::value net__view(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    std::string from = arg["user"];
    int num = arg.has("n") ? arg["n"] : -1;

    bool arr = requested_scripting_api(arg);

    if(from == "")
        from = get_caller(vctx);

    if(from == "")
        return js::make_error(vctx, "usage: net.view({user:<username>, n:-1})");

    if(num < 0)
        num = 15;

    if(num < 0 || num > 15)
        return js::make_error(vctx, "n out of range [1,15]");

    auto opt_user_and_nodes = get_user_and_nodes(from, get_thread_id(vctx));

    if(!opt_user_and_nodes.has_value())
        return js::make_error(vctx, "User does not exist");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(!playspace_network_manage.has_accessible_path_to(vctx, from, get_caller(vctx), (path_info::path_info)(path_info::VIEW_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
        return js::make_error(vctx, "Target Inaccessible");

    user& usr = opt_user_and_nodes->first;

    auto hostile_actions = opt_user_and_nodes->second.valid_hostile_actions();

    if(!usr.is_allowed_user(get_caller(vctx)) && usr.name != get_caller(vctx) && !((hostile_actions & user_node_info::VIEW_LINKS) > 0))
        return js::make_error(vctx, "Node is Locked");

    network_accessibility_info info = playspace_network_manage.generate_network_accessibility_from(vctx, from, num);


    ///so
    ///the information we want to give back to the client wants to be very rich
    ///we need the connections of every npc if we have permission
    ///path to original player? unsure on this
    ///position

    using nlohmann::json;

    std::vector<json> all_npc_data = get_net_view_data_arr(info);

    json final_data = all_npc_data;

    if(arr)
        return js::make_value(vctx, final_data);
    else
    {
        std::string str = get_net_view_data_str(all_npc_data);

        return js::make_value(vctx, str);
    }
}

#ifdef OLD_DEPRECATED
duk_ret_t net__access(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name = duk_safe_get_prop_string(ctx, -1, "user");

    if(name == "")
        return push_error(ctx, "Usage: net.access({user:<name>})");

    ///ok
    ///need to provide options

    ///valid options:
    ///add_user
    ///remove_user
    ///if no options, print userlist

    std::optional opt_user_and_nodes = get_user_and_nodes(name, get_thread_id(ctx));

    if(!opt_user_and_nodes.has_value())
        return push_error(ctx, "No such user");

    user& usr = opt_user_and_nodes->first;

    auto valid_actions = opt_user_and_nodes->second.valid_hostile_actions();

    if(!usr.is_allowed_user(get_caller(ctx)) && (valid_actions & user_node_info::CLAIM_NPC) == 0)
        return push_error(ctx, "Cannot access control panel, insufficient permissions or full user");

    ///anything past this point should probably force a payment of 200 credits


    std::string add_user = duk_safe_get_prop_string(ctx, -1, "add_user");
    std::string remove_user = duk_safe_get_prop_string(ctx, -1, "remove_user");
    bool view_users = dukx_is_prop_truthy(ctx, -1, "view_users");
    bool confirm = dukx_is_prop_truthy(ctx, -1, "confirm");

    std::vector<std::string> allowed_users = opt_user_and_nodes->first.get_allowed_users();

    std::string price_str = "Price: 200\n";

    if(usr.is_allowed_user(get_caller(ctx)))
        price_str = "Price: Free\n";

    std::string commands = "Usage: add_user:<username>, remove_user:<username>, view_users:true\n" + price_str;

    std::string situation_string = "Location: [" + std::to_string((int)usr.pos.v[0]) + ", " + std::to_string((int)usr.pos.v[1]) + ", " + std::to_string((int)usr.pos.v[2]) + "]\n";

    commands += situation_string;

    std::string sector_string = "Sector: " + usr.fetch_sector();

    commands += sector_string;

    if(add_user.size() == 0 && remove_user.size() == 0 && view_users)
    {
        if(!usr.is_allowed_user(get_caller(ctx)) && handle_confirmed(ctx, confirm, get_caller(ctx), 200))
            return 1;

        std::string ret;

        ret += "Authed Users:\n";

        for(auto& i : allowed_users)
        {
            ret += i + "\n";
        }

        push_duk_val(ctx, ret);

        return 1;
    }

    if(add_user.size() > 0 || remove_user.size() > 0)
    {
        if(add_user.size() > 0)
        {
            if(!get_user(add_user, get_thread_id(ctx)).has_value())
                return push_error(ctx, "Invalid add_user username");
        }

        if(remove_user.size() > 0)
        {
            if(!get_user(remove_user, get_thread_id(ctx)).has_value())
                return push_error(ctx, "Invalid remove_user username");
        }

        if(add_user.size() > 0 && (usr.all_found_props.get_prop_as_integer("is_user") == 1 || usr.auth != ""))
            return push_error(ctx, "Cannot take over a user");

        ///should be free if we're an allowed user
        if(!usr.is_allowed_user(get_caller(ctx)) && handle_confirmed(ctx, confirm, get_caller(ctx), 200))
            return 1;

        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(add_user.size() > 0)
        {
            usr.add_allowed_user(add_user, mongo_ctx);
        }

        if(remove_user.size() > 0)
        {
            usr.remove_allowed_user(remove_user, mongo_ctx);
        }

        usr.overwrite_user_in_db(mongo_ctx);

        return push_success(ctx, "Success");
    }

    if(add_user.size() == 0 && remove_user.size() == 0 && !view_users)
    {
        push_duk_val(ctx, commands);
        return 1;
    }

    return 0;
}
#endif // 0

duk_ret_t net__switch(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string target = duk_safe_get_prop_string(ctx, -1, "user");
    bool has_local = dukx_is_prop_truthy(ctx, -1, "local");

    if(target == "")
        return push_error(ctx, "Usage: net.switch({user:<username>})");

    std::vector<std::string> full_caller_stack = get_caller_stack(ctx);

    std::optional opt_user = get_user(full_caller_stack.front(), get_thread_id(ctx));

    if(!opt_user.has_value())
        return push_error(ctx, "Invalid username (host)");

    /*for(auto& i : full_caller_stack)
    {
        std::cout << "stk " << i << std::endl;
    }*/

    std::optional switch_to = get_user(target, get_thread_id(ctx));

    if(!switch_to.has_value())
        return push_error(ctx, "Invalid username (target)");

    if(!switch_to->is_npc() && switch_to->name != opt_user->name)
        return push_error(ctx, "Cannot switch to a user");

    opt_user->cleanup_call_stack(get_thread_id(ctx));

    ///so say we switched from i20k -> f_sdfdf
    ///call stack would be [i20k, f_sddfdf]
    ///and we'd be masquerading under the latter
    ///so. If any member of our call stack is on the permissions list, we're good to go
    std::vector<std::string> call_stack = opt_user->get_call_stack();

    //#ifndef TESTING
    bool found = false;

    for(auto it = call_stack.begin(); it != call_stack.end(); it++)
    {
        if(switch_to->is_allowed_user(*it))
        {
            call_stack.resize(std::distance(call_stack.begin(), it) + 1);
            call_stack.push_back(switch_to->name);

            found = true;
            break;
        }
    }
    //#endif // TESTING

    if(call_stack.size() == 0)
    {
        printf("weird call stack error 0\n");
        return 0;
    }

    //#ifndef TESTING
    if(!found)
    {
        if(switch_to->name == opt_user->name)
            call_stack = std::vector<std::string>{opt_user->name};
        else
            return push_error(ctx, "Insufficient permissions");
    }
    //#else // TESTING
    //call_stack.push_back(switch_to->name);
    //#endif // TESTING

    if(call_stack.size() > 0)
        call_stack.erase(call_stack.begin());

    user& usr = opt_user.value();

    usr.call_stack = call_stack;

    ///local:true means only for script call
    if(!has_local)
    {
        mongo_lock_proxy user_db = get_global_mongo_user_info_context(get_thread_id(ctx));

        usr.overwrite_user_in_db(user_db);
    }

    duk_push_heap_stash(ctx);

    ///new caller
    quick_register(ctx, "caller", switch_to->name.c_str());
    quick_register_generic(ctx, "caller_stack", usr.get_call_stack());

    duk_pop_n(ctx, 1);

    ///need to update caller and caller_stack

    return push_success(ctx, "Success");
}

/*namespace task_type
{
    enum task_type
    {
        LINK_XFER
    };
}

void schedule_task(const std::string& user, const std::string& type, float duration)
{

}*/

duk_ret_t net__move(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string host = duk_safe_get_prop_string(ctx, -1, "user");
    std::string target = duk_safe_get_prop_string(ctx, -1, "target");

    if(host == "")
        return push_error(ctx, "Usage: net.move({user:<username>, target:<username>})");

    if(target == "")
        return push_error(ctx, "Usage: net.move({user:<username>, target:<username>})");

    std::optional opt_user = get_user(host, get_thread_id(ctx));

    if(!opt_user.has_value())
        return push_error(ctx, "Invalid username (user)");

    std::optional opt_target = get_user(target, get_thread_id(ctx));

    if(!opt_target.has_value())
        return push_error(ctx, "Invalid username (target)");

    user u1 = opt_user.value();

    if(!u1.is_allowed_user(get_caller(ctx)) && u1.name != get_caller(ctx))
        return push_error(ctx, "No permission for user (user)");

    if(!opt_target->is_allowed_user(get_caller(ctx)) && opt_target->name != get_caller(ctx))
        return push_error(ctx, "No permission for user (target)");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(!playspace_network_manage.could_link(target, host))
        return push_error(ctx, "User and Target not in the same system");

    float link_stability_cost = 50;

    js::value_context vctx(ctx);

    std::vector<std::string> path = playspace_network_manage.get_accessible_path_to(vctx, target, host, path_info::USE_LINKS, -1, link_stability_cost);

    if(path.size() == 0)
    {
        auto pseudo_path = playspace_network_manage.get_accessible_path_to(vctx, target, host, path_info::USE_LINKS, -1);

        if(pseudo_path.size() != 0)
            return push_error(ctx, "Path insufficiently stable, each link must have at least 50 stability");
        else
            return push_error(ctx, "No path");
    }

    double dist = (opt_user->pos - opt_target->pos).length();

    float cost_mult = 0.1f;

    double cost = path.size() * 100 + dist;

    cost *= cost_mult;

    bool confirm = duk_safe_get_generic(dukx_is_truthy, ctx, -1, "confirm", false);

    float temp_time_s = 10 * path.size() + dist/10.;

    if(!confirm)
    {
        std::string str = "Please confirm:true to pay " + std::to_string(cost) + " for a travel trip of " + std::to_string(path.size()) + " links across " + std::to_string(dist) + " ERR(s)\n";

        str += "Time: " + std::to_string(temp_time_s) + "(s)";

        push_duk_val(ctx, str);

        return 1;
    }

    if(playspace_network_manage.current_network_links(opt_target->name) >= playspace_network_manage.max_network_links(opt_target->name))
        return push_error(ctx, "Target has no free link slots");

    if(take_cash(ctx, get_caller(ctx), cost) != 0)
        return 1;

    scheduled_tasks& tasks = get_global_scheduled_tasks();

    tasks.task_register(task_type::ON_RELINK, temp_time_s, path, get_thread_id(ctx));

    std::string ret = "Relinking will finish in " + std::to_string(temp_time_s) + " seconds\n";

    for(auto& i : path)
    {
        ret += i + " ";
    }

    push_duk_val(ctx, ret);

    ///so
    ///path leaking
    ///the obvious choice to make is whether or not it should leak into the breach or front node
    ///aka, when you're hacking to look for leaked paths from a move, should you trigger a probing script or not?
    ///decision is that yes it should trigger probes

    return 1;
}

///from a gameplay perspective it'd be nice to see the stability between two points
///with cash causing stability loss along a path, itll be possible to use that to trace people (which is very much suboptimal)
///lets say that I fix that though
///so. Path tells you link stability between first and last node
///what gameplay ramifications does this have?
///Ok so. If we have view perms this will give path, otherwise itll just give link stability
///with optional minimum stability
duk_ret_t net__path(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    float minimum_stability = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "min_stability", 0.f);

    std::string start = duk_safe_get_prop_string(ctx, -1, "user");
    std::string target = duk_safe_get_prop_string(ctx, -1, "target");
    int arr = dukx_is_prop_truthy(ctx, -1, "array");

    std::string path_type = duk_safe_get_prop_string(ctx, -1, "type");

    if(!duk_has_prop_string(ctx, -1, "type"))
        path_type = "view";

    if(path_type != "view" && path_type != "use")
        return push_error(ctx, "type must be view or use");

    if(start == "")
        start = get_caller(ctx);

    if(target == "")
        return push_error(ctx, "Requires target:<username> argument");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    js::value_context vctx(ctx);

    if(!playspace_network_manage.has_accessible_path_to(vctx, start, get_caller(vctx), path_info::VIEW_LINKS))
        return push_error(ctx, "No path to start user");

    std::vector<std::string> viewable_distance;

    if(path_type == "view")
        viewable_distance = playspace_network_manage.get_accessible_path_to(vctx, target, start, path_info::VIEW_LINKS, -1, minimum_stability);
    else
        viewable_distance = playspace_network_manage.get_accessible_path_to(vctx, target, start, path_info::USE_LINKS, -1, minimum_stability);

    ///STRANGER DANGER
    std::vector<std::string> link_path = playspace_network_manage.get_accessible_path_to(vctx, target, start, (path_info::path_info)(path_info::NONE | path_info::ALLOW_WARP_BOUNDARY | path_info::TEST_ACTION_THROUGH_WARP_NPCS), -1, minimum_stability);

    //float total_path_strength = playspace_network_manage.get_total_path_link_strength(link_path);

    //float avg_path_strength = 0.f;

    //if(link_path.size() > 0)
    //    avg_path_strength = total_path_strength / (float)link_path.size();

    float minimum_path_strength = playspace_network_manage.get_minimum_path_link_strength(link_path);

    std::string visible_path = path_type + " path: ";

    visible_path[0] = toupper(visible_path[0]);

    if(viewable_distance.size() == 0)
        visible_path += "Unknown";
    else
    {
        for(int i=0; i < (int)viewable_distance.size(); i++)
        {
            if(i < (int)viewable_distance.size() - 1)
                visible_path += viewable_distance[i] + " -> ";
            else
                visible_path += viewable_distance[i];
        }
    }

    std::string path_stab = "Direct Path Stability: ";

    if(link_path.size() == 0)
    {
        path_stab += "Unknown";
    }
    else
    {
        path_stab += to_string_with_enforced_variable_dp(minimum_path_strength, 2) + " [min]";

        //path_stab += to_string_with_enforced_variable_dp(total_path_strength, 2) + " [total], " +
        //             to_string_with_enforced_variable_dp(avg_path_strength, 2) + " [avg]";
    }

    if(!arr)
    {
        push_duk_val(ctx, visible_path + "\n" + path_stab);
    }
    else
    {
        using nlohmann::json;

        json j;

        j["path"] = viewable_distance;
        //j["total_stability"] = total_path_strength;
        //j["avg_stability"] = avg_path_strength;

        j["min_stability"] = minimum_path_strength;

        push_duk_val(ctx, j);
    }

    return 1;
}

#if 0
duk_ret_t try_create_new_link(duk_context* ctx, const std::string& user_1, const std::string& user_2, double stab, double price)
{
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    auto opt_user_and_nodes_1 = get_user_and_nodes(user_1, get_thread_id(ctx));
    auto opt_user_and_nodes_2 = get_user_and_nodes(user_2, get_thread_id(ctx));

    if(!opt_user_and_nodes_1.has_value() || !opt_user_and_nodes_2.has_value())
        return push_error(ctx, "Invalid User");

    user_nodes& n1 = opt_user_and_nodes_1->second;
    user_nodes& n2 = opt_user_and_nodes_2->second;

    user& u1 = opt_user_and_nodes_1->first;
    user& u2 = opt_user_and_nodes_2->first;

    bool invalid_1 = !n1.is_valid_hostile_action(user_node_info::hostile_actions::USE_LINKS) && !u1.is_allowed_user(get_caller(ctx)) && u1.name != get_caller(ctx);
    bool invalid_2 = !n2.is_valid_hostile_action(user_node_info::hostile_actions::USE_LINKS) && !u2.is_allowed_user(get_caller(ctx)) && u2.name != get_caller(ctx);

    if(invalid_1 || invalid_2)
        return push_error(ctx, "Breach Node Secured");

    if(stab <= 0)
        return push_error(ctx, "Cannot create a new link with stability <= 0");

    std::string rstr;

    if(price <= 0)
    {
        rstr += stab_str;
        rstr += no_stab_str;

        push_duk_val(ctx, rstr);
        return 1;
    }
    else
    {
        vec3f vdist = (u2.pos - u1.pos);

        double dist = vdist.length();

        ///30 cash per network unit
        price += (dist / ESEP) * 30.f;
    }

    if(price != 0)
    {
        if(handle_confirmed(ctx, confirm, get_caller(ctx), price))
            return 1;

        if(playspace_network_manage.current_network_links(usr) >= playspace_network_manage.max_network_links(usr) ||
           playspace_network_manage.current_network_links(target) >= playspace_network_manage.max_network_links(target))
            return push_error(ctx, "No spare links");

        scheduled_tasks& task_sched = get_global_scheduled_tasks();

        task_sched.task_register(task_type::ON_HEAL_NETWORK, 10.f, {usr, target, std::to_string(stab)}, get_thread_id(ctx));

        //return push_success(ctx, "Link creation scheduled in 10s");
        return 0;
    }

    return push_error(ctx, "Nothing To Do");
}
#endif // 0

js::value create_and_modify_link(js::value_context& vctx, const std::string& from, const std::string& user_1, const std::string& target, bool create, double stab, bool confirm, bool enforce_connectivity, std::string path_type = "use")
{
    std::optional opt_user_and_nodes_1 = get_user_and_nodes(user_1, get_thread_id(vctx));
    std::optional opt_user_and_nodes_2 = get_user_and_nodes(target, get_thread_id(vctx));

    if(!opt_user_and_nodes_1.has_value())
        return js::make_error(vctx, "No such user (user)");

    if(!opt_user_and_nodes_2.has_value())
        return js::make_error(vctx, "No such user (target)");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(!create && !playspace_network_manage.has_accessible_path_to(vctx, user_1, from, (path_info::path_info)(path_info::VIEW_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
        return js::make_error(vctx, "No currently visible path to user");

    //double stab = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "delta", 0);
    //std::string path_type = duk_safe_get_prop_string(ctx, -1, "type");

    if(isnanf(stab))
        stab = 0;

    for(auto& i : path_type)
    {
        i = std::tolower(i);
    }

    if(path_type == "")
        path_type = "use";

    if(enforce_connectivity)
    {
        if(!playspace_network_manage.has_accessible_path_to(vctx, target, user_1, (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
            return js::make_error(vctx, "No path from user to target");
    }

    float link_stability_for_one_cash = 1.f;
    float link_stability_to_cash = 1.f/link_stability_for_one_cash;

    std::string stab_str = "1 link stability : " + std::to_string(link_stability_to_cash) + " cash\n";
    std::string no_stab_str = "No stability delta requested, please input " + make_key_col("delta") + ":" + make_val_col("num");
    float price = fabs(stab * link_stability_to_cash);

    if(!create)
    {
        std::string rstr = "Change connection strength or pass " + make_key_col("create") + ":" + make_val_col("true") + " to attempt new linkage\n";

        if(price == 0)
        {
            rstr += stab_str;
            rstr += "Select Path " + make_key_col("type") + ":" + make_val_col("view") + " or " + make_val_col("use") + " (selected " + path_type + ")\n";
            rstr += no_stab_str;
        }

        if(price != 0)
        {
            if(auto res = handle_confirmed(vctx, confirm, get_caller(vctx), price); res.has_value())
                return res.value();

            std::vector<std::string> path;

            //if(path_type == "direct")
            //    path = playspace_network_manage.get_accessible_path_to(ctx, target, usr, path_info::NONE);
            if(path_type == "view")
                path = playspace_network_manage.get_accessible_path_to(vctx, target, user_1, path_info::VIEW_LINKS);
            if(path_type == "use")
                path = playspace_network_manage.get_accessible_path_to(vctx, target, user_1, path_info::USE_LINKS);

            if(path.size() == 0)
                return js::make_error(vctx, "No path");

            user_log next;
            next.add("type", "path_fortify", "");

            playspace_network_manage.modify_path_per_link_strength_with_logs(path, stab / ((float)path.size() - 1.f) , {next}, get_thread_id(vctx));

            return js::make_success(vctx);
            //return push_success(ctx, "Distributed " + std::to_string(stab) + " across " + std::to_string((int)path.size() - 1) + " links");
        }

        return js::make_value(vctx, rstr);
    }
    else
    {
        user_nodes& n1 = opt_user_and_nodes_1->second;
        user_nodes& n2 = opt_user_and_nodes_2->second;

        user& u1 = opt_user_and_nodes_1->first;
        user& u2 = opt_user_and_nodes_2->first;

        bool invalid_1 = !n1.is_valid_hostile_action(user_node_info::hostile_actions::USE_LINKS) && !u1.is_allowed_user(get_caller(vctx)) && u1.name != get_caller(vctx);
        bool invalid_2 = !n2.is_valid_hostile_action(user_node_info::hostile_actions::USE_LINKS) && !u2.is_allowed_user(get_caller(vctx)) && u2.name != get_caller(vctx);

        if(invalid_1 || invalid_2)
            return js::make_error(vctx, "Breach Node Secured");

        if(stab <= 0)
            return js::make_error(vctx, "Cannot create a new link with stability <= 0");

        std::string rstr;

        if(price <= 0)
        {
            rstr += stab_str;
            rstr += no_stab_str;

            return js::make_value(vctx, rstr);
        }
        else
        {
            vec3f vdist = (u2.get_local_pos() - u1.get_local_pos());

            double dist = vdist.length();

            ///30 cash per network unit
            price += (dist / ESEP) * 20.f;
        }

        if(price != 0)
        {
            if(auto res = handle_confirmed(vctx, confirm, get_caller(vctx), price); res.has_value())
                return res.value();

            if(playspace_network_manage.current_network_links(user_1) >= playspace_network_manage.max_network_links(user_1) ||
               playspace_network_manage.current_network_links(target) >= playspace_network_manage.max_network_links(target))
                return js::make_error(vctx, "No spare links");

            scheduled_tasks& task_sched = get_global_scheduled_tasks();

            task_sched.task_register(task_type::ON_HEAL_NETWORK, 10.f, {user_1, target, std::to_string(stab)}, get_thread_id(vctx));

            return js::make_success(vctx);
        }

        return js::make_value(vctx, "Schedule new connection?");
    }
}


#ifdef OLD_DEPRECATED
duk_ret_t net__modify(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string usr = duk_safe_get_prop_string(ctx, -1, "user");
    std::string target = duk_safe_get_prop_string(ctx, -1, "target");

    if(target == "")
        return push_error(ctx, "Requires a target:<username> parameter");

    if(usr == "")
        usr = get_caller(ctx);

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    std::optional opt_user_and_nodes_1 = get_user_and_nodes(usr, get_thread_id(ctx));
    std::optional opt_user_and_nodes_2 = get_user_and_nodes(target, get_thread_id(ctx));

    if(!opt_user_and_nodes_1.has_value())
        return push_error(ctx, "No such user (user)");

    if(!opt_user_and_nodes_2.has_value())
        return push_error(ctx, "No such user (target)");

    if(!playspace_network_manage.could_link(usr, target))
        return push_error(ctx, "User and Target not in same system");

    bool confirm = dukx_is_prop_truthy(ctx, -1, "confirm");
    bool create = dukx_is_prop_truthy(ctx, -1, "create");

    ///the requirements for being able to strengthen two points in the network is that we have vision on the first
    ///and then either use or view perms to the second
    if(!create && !playspace_network_manage.has_accessible_path_to(ctx, usr, get_caller(ctx), path_info::VIEW_LINKS))
        return push_error(ctx, "No currently visible path to user");

    double stab = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "delta", 0);
    std::string path_type = duk_safe_get_prop_string(ctx, -1, "type");

    if(isnanf(stab))
        stab = 0;

    for(auto& i : path_type)
    {
        i = std::tolower(i);
    }

    if(path_type == "")
        path_type = "use";

    float link_stability_for_one_cash = 1.f;
    float link_stability_to_cash = 1.f/link_stability_for_one_cash;

    std::string stab_str = "1 link stability : " + std::to_string(link_stability_to_cash) + " cash\n";
    std::string no_stab_str = "No stability delta requested, please input " + make_key_col("delta") + ":" + make_val_col("num");
    float price = fabs(stab * link_stability_to_cash);

    if(!create)
    {
        std::string rstr = "Change connection strength or pass " + make_key_col("create") + ":" + make_val_col("true") + " to attempt new linkage\n";

        if(price == 0)
        {
            rstr += stab_str;
            rstr += "Select Path " + make_key_col("type") + ":" + make_val_col("view") + " or " + make_val_col("use") + " (selected " + path_type + ")\n";
            rstr += no_stab_str;
        }

        if(price != 0)
        {
            if(handle_confirmed(ctx, confirm, get_caller(ctx), price))
                return 1;

            std::vector<std::string> path;

            //if(path_type == "direct")
            //    path = playspace_network_manage.get_accessible_path_to(ctx, target, usr, path_info::NONE);
            if(path_type == "view")
                path = playspace_network_manage.get_accessible_path_to(ctx, target, usr, path_info::VIEW_LINKS);
            if(path_type == "use")
                path = playspace_network_manage.get_accessible_path_to(ctx, target, usr, path_info::USE_LINKS);

            if(path.size() == 0)
                return push_error(ctx, "No path");

            playspace_network_manage.modify_path_per_link_strength_with_logs(path, stab / ((float)path.size() - 1.f) , {"Path Fortify"}, get_thread_id(ctx));

            return push_success(ctx, "Distributed " + std::to_string(stab) + " across " + std::to_string((int)path.size() - 1) + " links");
        }

        push_duk_val(ctx, rstr);
    }
    else
    {
        user_nodes& n1 = opt_user_and_nodes_1->second;
        user_nodes& n2 = opt_user_and_nodes_2->second;

        user& u1 = opt_user_and_nodes_1->first;
        user& u2 = opt_user_and_nodes_2->first;

        bool invalid_1 = !n1.is_valid_hostile_action(user_node_info::hostile_actions::USE_LINKS) && !u1.is_allowed_user(get_caller(ctx)) && u1.name != get_caller(ctx);
        bool invalid_2 = !n2.is_valid_hostile_action(user_node_info::hostile_actions::USE_LINKS) && !u2.is_allowed_user(get_caller(ctx)) && u2.name != get_caller(ctx);

        if(invalid_1 || invalid_2)
            return push_error(ctx, "Breach Node Secured");

        if(stab <= 0)
            return push_error(ctx, "Cannot create a new link with stability <= 0");

        std::string rstr;

        if(price <= 0)
        {
            rstr += stab_str;
            rstr += no_stab_str;

            push_duk_val(ctx, rstr);
            return 1;
        }
        else
        {
            vec3f vdist = (u2.pos - u1.pos);

            double dist = vdist.length();

            ///30 cash per network unit
            price += (dist / ESEP) * 30.f;
        }

        if(price != 0)
        {
            if(handle_confirmed(ctx, confirm, get_caller(ctx), price))
                return 1;

            if(playspace_network_manage.current_network_links(usr) >= playspace_network_manage.max_network_links(usr) ||
               playspace_network_manage.current_network_links(target) >= playspace_network_manage.max_network_links(target))
                return push_error(ctx, "No spare links");

            scheduled_tasks& task_sched = get_global_scheduled_tasks();

            task_sched.task_register(task_type::ON_HEAL_NETWORK, 10.f, {usr, target, std::to_string(stab)}, get_thread_id(ctx));

            return push_success(ctx, "Link creation scheduled in 10s");
        }

        push_duk_val(ctx, "Schedule new connection?");
    }

    return 1;
}
#endif // OLD_DEPRECATED

duk_ret_t cheats__task(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    scheduled_tasks& tasks = get_global_scheduled_tasks();

    tasks.task_register(task_type::ON_RELINK, 1, {"hello", "whythere"}, get_thread_id(ctx));

    return 0;
}

duk_ret_t cheats__disconnect(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string usr = duk_safe_get_prop_string(ctx, -1, "user");

    if(usr == "")
        return push_error(ctx, "ARGLBLARLGB");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    playspace_network_manage.unlink_all(usr);

    return 0;
}

duk_ret_t cheats__unlink(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string usr = duk_safe_get_prop_string(ctx, -1, "user");
    std::string usr2 = duk_safe_get_prop_string(ctx, -1, "user2");

    if(usr == "" || usr2 == "")
        return push_error(ctx, "ARGLBLARLGB");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    playspace_network_manage.unlink(usr, usr2);

    return 0;
}

bool elegible_for_loot_gen(user& usr);

duk_ret_t cheats__testloot(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string usr = duk_safe_get_prop_string(ctx, -1, "user");

    if(usr == "")
        return push_error(ctx, "ARGLBLARLGB");

    user found;

    {
        mongo_lock_proxy mctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!found.load_from_db(mctx, usr))
            return push_error(ctx, "no user");
    }

    push_duk_val(ctx, std::to_string(elegible_for_loot_gen(found)));

    return 1;
}

duk_ret_t gal__map(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    auto structs = get_global_structure();

    int w = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "w", 40);
    int h = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "h", 30);

    if(w < 0 || h < 0)
        return push_error(ctx, "w or h < 0");

    w = clamp(w, 2, 200);
    h = clamp(h, 2, 200);

    //std::vector<std::vector<std::string>> strs;

    std::vector<std::string> strs;
    strs.resize(h);

    for(int i=0; i < (int)h; i++)
    {
        strs[i].resize(w-1);

        for(auto& k : strs[i])
            k = ' ';

        strs[i].push_back('\n');
    }

    vec2f br = {-FLT_MAX, -FLT_MAX};
    vec2f tl = {FLT_MAX, FLT_MAX};

    for(auto& i : structs)
    {
        auto pos = i.get_centre();

        br = max(br, pos.xy());
        tl = min(tl, pos.xy());
    }

    for(auto& i : structs)
    {
        auto pos = i.get_centre();

        vec2f scaled = (pos.xy() - tl) / (br - tl);

        //std::cout << "pos " << pos << " br " << br << " tl " << tl << std::endl;

        vec2f real_size = scaled * (vec2f){w, h};

        real_size = clamp(real_size, (vec2f){0.f, 0.f}, (vec2f){w, h}-1.f);

        real_size = round(real_size);

        strs[(int)real_size.y()][(int)real_size.x()] = 'n';
    }

    std::string fin;

    for(auto& i : strs)
    {
        fin += i;
    }

    push_duk_val(ctx, fin);

    return 1;
}

std::string format_string(const std::string& str, const std::vector<std::string>& all)
{
    std::string ret = str;

    int max_str = 0;

    for(auto& i : all)
    {
        if((int)i.size() > max_str)
        {
            max_str = (int)i.size();
        }
    }

    for(int i=(int)ret.size(); i < max_str; i++)
    {
        ret += ' ';
    }

    return ret;
}

duk_ret_t gal__list(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");

    using nlohmann::json;

    std::vector<json> data;

    auto structs = get_global_structure();

    for(auto& i : structs)
    {
        auto pos = i.get_centre();

        json j;
        j["name"] = i.name;
        j["x"] = pos.x();
        j["y"] = pos.y();
        j["z"] = pos.z();

        data.push_back(j);
    }

    if(is_arr)
    {
        json parsed;
        parsed = data;

        duk_push_string(ctx, ((std::string)parsed.dump()).c_str());

        duk_json_decode(ctx, -1);
        return 1;
    }
    else
    {
        std::string str;

        std::vector<std::string> names;
        std::vector<std::string> positions;

        for(auto& i : data)
        {
            std::string str = i["name"];

            names.push_back("Name: " + (std::string)str);

            float x = i["x"];
            float y = i["y"];
            float z = i["z"];

            std::string xyz = "Position: [" + std::to_string((int)x) + ", " + std::to_string((int)y) + ", " + std::to_string((int)z) + "]";

            positions.push_back(xyz);
        }

        for(int i=0; i < (int)names.size(); i++)
        {
            str += format_string(names[i], names) + " | " + format_string(positions[i], positions) + "\n";
        }

        duk_push_string(ctx, str.c_str());
    }

    return 1;
}

///need to centre sys.map on player by default
duk_ret_t sys__map(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    #ifdef TESTING
    /*MAKE_PERF_COUNTER();
    mongo_diagnostics diagnostic_scope;*/
    #endif // TESTING

    bool centre = dukx_is_prop_truthy(ctx, -1, "centre");

    user my_user;

    {
        mongo_nolock_proxy lock = get_global_mongo_user_info_context(-2);

        if(!my_user.load_from_db(lock, get_caller(ctx)))
            return push_error(ctx, "Error: Does not exist");
    }

    int n_val = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", -1);
    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");
    //int found_width = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", -1);

    int found_w = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "w", 160);
    int found_h = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "h", 80);

    bool by_seclevel = dukx_is_prop_truthy(ctx, -1, "seclevels") || dukx_is_prop_truthy(ctx, -1, "seclevel") || dukx_is_prop_truthy(ctx, -1, "s");

    std::string extra_args = "Pass " + make_key_val("seclevel", "true") + " to display seclevels\n";

    found_w = clamp(found_w, 5, 300);
    found_h = clamp(found_h, 5, 200);

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    auto opt_sys = low_level_structure_manage.get_system_of(my_user);

    if(!opt_sys.has_value())
        return push_error(ctx, "Well then, you are lost!");

    low_level_structure& structure = *opt_sys.value();

    std::vector<low_level_structure>& systems = low_level_structure_manage.systems;

    auto buffer = ascii_make_buffer({found_w, found_h}, false);

    network_accessibility_info info;

    if(n_val > 0)
    {
        if(n_val > 50)
            n_val = 50;

        std::map<int, std::deque<low_level_structure*>> to_test{{0, {&structure}}};
        std::map<std::string, bool> explored;

        int current_ring = 0;

        while(current_ring < n_val && to_test[current_ring].size() > 0)
        {
            low_level_structure* next = to_test[current_ring].front();

            explored[next->name] = true;

            info.rings[next->name] = current_ring;
            info.global_pos[next->name] = next->get_pos();
            info.ring_ordered_names.push_back(next->name);

            to_test[current_ring].pop_front();

            std::vector<std::string> found = next->get_connected_systems();

            for(auto& name : found)
            {
                if(explored[name])
                    continue;

                auto opt_structure = low_level_structure_manage.get_system_from_name(name);

                if(!opt_structure.has_value())
                    continue;

                to_test[current_ring + 1].push_back(opt_structure.value());
            }

            if(to_test[current_ring].size() == 0)
            {
                current_ring++;
            }
        }
    }
    else
    {
        for(auto& i : systems)
        {
            info.rings[i.name] = 0;
            info.global_pos[i.name] = i.get_pos();
            info.ring_ordered_names.push_back(i.name);
        }
    }

    info.keys.clear();
    info.display_string.clear();

    int count = 0;
    for(auto& i : info.ring_ordered_names)
    {
        ///name, character
        info.keys.push_back({i, ascii_index_to_full_character(count)});
        info.display_string[i] = ascii_index_to_full_character(count);

        count++;
    }

    vec3f pos = {0,0,0};

    if(centre)
        pos = structure.get_pos();

    //std::cout << *low_level_structure_manage.get_system_of(my_user).value()->name << std::endl;

    if(!is_arr)
    {
        ascii::ascii_render_flags flags = ascii::USE_SYS;

        if(!centre)
            flags = (ascii::ascii_render_flags)(flags | ascii::FIT_TO_AREA);

        flags = (ascii::ascii_render_flags)(flags | ascii::HIGHLIGHT_USER);

        if(by_seclevel)
        {
            flags = (ascii::ascii_render_flags)(flags | ascii::COLOUR_BY_SECLEVEL);
        }

        std::string result = ascii_render_from_accessibility_info(info, buffer, pos, 0.07f, flags, structure.name);

        result = "Current Sys: " + colour_string(structure.name) + "\n" + result;

        push_duk_val(ctx, extra_args + result);
    }
    else
    {
        nlohmann::json all_data;

        std::vector<nlohmann::json> data;

        if(info.ring_ordered_names.size() == 0 && n_val > 0)
        {
            std::cout << "repro ghamb bug\n";

            auto low_level_opt = low_level_structure_manage.get_system_of(my_user);

            if(low_level_opt.has_value())
            {
                info.ring_ordered_names.push_back(low_level_opt.value()->name);
                info.global_pos[low_level_opt.value()->name] = low_level_opt.value()->get_pos();
            }
        }

        for(auto& i : info.ring_ordered_names)
        {
            nlohmann::json j;

            std::string name = i;

            auto opt_structure = low_level_structure_manage.get_system_from_name(name);

            if(!opt_structure.has_value())
                continue;

            low_level_structure& structure = *opt_structure.value();

            vec3f pos = info.global_pos[i];
            std::vector<std::string> links = structure.get_connected_systems();

            j["name"] = name;
            j["x"] = pos.x();
            j["y"] = pos.y();
            j["z"] = pos.z();
            j["links"] = links;
            j["seclevel"] = structure.calculate_seclevel();

            data.push_back(j);
        }

        if(data.size() == 0 && n_val > 0)
        {
            std::cout << "repro ghamb bug pt 2\n";
        }

        all_data = data;

        push_duk_val(ctx, all_data);
    }

    return 1;
}

#ifdef SYSTEM_TESTING
duk_ret_t sys__debug_view(priv_context& priv_ctx, duk_context* ctx, int sl)
{

    //std::string str = duk_safe_get_prop_string(ctx, -1, "sys");
    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");
    std::string found_target = duk_safe_get_prop_string(ctx, -1, "user");

    bool has_fit = dukx_is_prop_truthy(ctx, -1, "fit");

    int found_w = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "w", 80);
    int found_h = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "h", 40);

    float found_scale = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "scale", 1.f);

    found_w = clamp(found_w, 5, 300);
    found_h = clamp(found_h, 5, 200);

    bool has_target = found_target.size() > 0;

    if(found_target == "")
        found_target = get_caller(ctx);

    if(!dukx_is_prop_truthy(ctx, -1, "scale"))
    {
        if(has_target)
            found_scale = 0.5f;
        else
            found_scale = 0.5f;
    }

    found_scale = clamp(found_scale, 0.1f, 10.f);

    user target_user;

    {
        mongo_nolock_proxy lock = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!target_user.load_from_db(lock, found_target))
            return push_error(ctx, "Error: Target does not exist");
    }

    //low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    user my_user;

    {
        mongo_nolock_proxy lock = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!my_user.load_from_db(lock, get_caller(ctx)))
            return push_error(ctx, "Error: Does not exist");
    }

    ///disables cross system stuff
    //if(!target_user.is_allowed_user(my_user.name) && !low_level_structure_manage.in_same_system(target_user.name, my_user.name))
    //    return push_error(ctx, "Cannot sys.view cross systems on non owned users");

    int n_count = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", -1);

    if(n_count < -1)
        n_count = -1;

    if(n_count > 99)
        n_count = 99;

    std::vector<std::vector<std::string>> buffer = ascii_make_buffer({found_w, found_h}, false);

    js::value_context vctx(ctx);

    network_accessibility_info info;

    network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(vctx, target_user.name, n_count, (path_info::path_info)(path_info::NONE | path_info::SKIP_SYSTEM_CHECKS));

    info = network_accessibility_info::merge_together(info, cur);

    auto links = playspace_network_manage.get_links(target_user.name);

    for(auto& i : links)
        std::cout << "CNUM " << i << std::endl;

    if(!is_arr)
    {
        std::string from = get_caller(ctx);

        vec3f pos = {0,0,0};

        if(has_target)
            pos = info.global_pos[target_user.name];

        ascii::ascii_render_flags flags = ascii::NONE;

        if(has_fit)
        {
            flags = (ascii::ascii_render_flags)(flags | ascii::FIT_TO_AREA | ascii::HIGHLIGHT_USER);
        }

        std::string result = ascii_render_from_accessibility_info(info, buffer, pos, found_scale, flags, target_user.name);

        std::string seclevel_string = "Seclevel: ";

        int seclevel = 0;
        std::string sstring = seclevel_to_string(seclevel);

        std::string col = string_to_colour(sstring);

        seclevel_string += "(`" + col + to_string_with_enforced_variable_dp(seclevel, 2) + "` - `" + col + sstring + "`)";

        result = "Current Sys: " + colour_string("dummy") + "\n" + seclevel_string + "\n" + result;

        push_duk_val(ctx, result);
    }

    return 1;
}

#endif // SYSTEM_TESTING

js::value sys__view(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    #ifdef TESTING
    /*MAKE_PERF_COUNTER();
    mongo_diagnostics diagnostic_scope;*/
    #endif // TESTING

    //std::string str = duk_safe_get_prop_string(ctx, -1, "sys");
    bool is_arr = requested_scripting_api(arg);
    std::string found_target = arg["user"];

    bool has_fit = arg["fit"].is_truthy();

    int found_w = arg.has("w") ? arg["w"] : 80;
    int found_h = arg.has("h") ? arg["h"] : 40;

    float found_scale = arg.has("scale") ? arg["scale"] : 1;

    found_w = clamp(found_w, 5, 300);
    found_h = clamp(found_h, 5, 200);

    bool has_target = found_target.size() > 0;

    if(found_target == "")
        found_target = get_caller(vctx);

    if(!arg["scale"].is_truthy())
    {
        if(has_target)
            found_scale = 0.5f;
        else
            found_scale = 0.5f;
    }

    found_scale = clamp(found_scale, 0.1f, 10.f);

    user target_user;

    {
        mongo_nolock_proxy lock = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!target_user.load_from_db(lock, found_target))
            return js::make_error(vctx, "Error: Target does not exist");
    }

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    user my_user;

    {
        mongo_nolock_proxy lock = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!my_user.load_from_db(lock, get_caller(vctx)))
            return js::make_error(vctx, "Error: Does not exist");
    }

    ///disables cross system stuff
    if(!target_user.is_allowed_user(my_user.name) && !low_level_structure_manage.in_same_system(target_user.name, my_user.name))
        return js::make_error(vctx, "Cannot sys.view cross systems on non owned users");

    int n_count = arg.has("n") ? arg["n"] : -1;

    if(n_count < -1)
        n_count = -1;

    if(n_count > 99)
        n_count = 99;

    std::optional<low_level_structure*> opt_structure;

    //if(str == "")
        opt_structure = low_level_structure_manage.get_system_of(my_user);
    /*else
        opt_structure = low_level_structure_manage.get_system_from_name(str);*/

    if(!opt_structure.has_value())
        return js::make_error(vctx, "You are lost, there is no help for you now");

    /*std::cout <<" tlkinks " << playspace_network_manage.current_network_links(my_user.name) << std::endl;

    auto my_links = playspace_network_manage.get_links(my_user.name);

    for(auto& i : my_links)
    {
        std::cout << "fl " << i << std::endl;
    }*/

    low_level_structure& structure = *opt_structure.value();

    std::vector<user> special_users = load_users_nolock(structure.get_special_users());
    std::vector<user> all_users;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));
        all_users = structure.get_all_users(mongo_ctx);
    }

    std::vector<std::vector<std::string>> buffer = ascii_make_buffer({found_w, found_h}, false);

    network_accessibility_info info;

    bool visited_host_user = false;

    if(playspace_network_manage.current_network_links(my_user.name) == 0)
    {
        for(user& usr : special_users)
        {
            network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(vctx, usr.name, n_count);

            info = network_accessibility_info::merge_together(info, cur);

            if(usr.name == target_user.name)
                visited_host_user = true;
        }

        ///investigate this for being incredibly terrible
        for(user& usr : all_users)
        {
            if(playspace_network_manage.current_network_links(usr.name) > 0)
                continue;

            network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(vctx, usr.name, n_count);

            info = network_accessibility_info::merge_together(info, cur);

            if(usr.name == target_user.name)
                visited_host_user = true;
        }
    }

    if(!visited_host_user)
    {
        network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(vctx, target_user.name, n_count);

        info = network_accessibility_info::merge_together(info, cur);
    }

    for(auto& user_name : info.ring_ordered_names)
    {
        if(npc_info::has_type_fast_locking(npc_info::WARPY, user_name))
        {
            std::vector<std::string> connected = structure.get_connected_systems_from(user_name);

            if(connected.size() > 0)
            {
                info.extra_data_map[user_name] += "(";
            }

            for(int i=0; i < (int)connected.size(); i++)
            {
                if(i != (int)connected.size() - 1)
                    info.extra_data_map[user_name] += colour_string(connected[i]) + ", ";
                else
                    info.extra_data_map[user_name] += colour_string(connected[i]);
            }

            if(connected.size() > 0)
            {
                info.extra_data_map[user_name] += ")";
            }
        }

        if(playspace_network_manage.current_network_links(user_name) == 0)
        {
            info.extra_data_map[user_name] += "(free)";
        }
    }

    if(!is_arr)
    {
        std::string from = get_caller(vctx);

        vec3f pos = {0,0,0};
        ///info.global_pos[from]

        if(has_target)
            pos = info.global_pos[target_user.name];

        ascii::ascii_render_flags flags = ascii::NONE;

        if(has_fit)
        {
            flags = (ascii::ascii_render_flags)(flags | ascii::FIT_TO_AREA | ascii::HIGHLIGHT_USER);
        }

        std::string result = ascii_render_from_accessibility_info(info, buffer, pos, found_scale, flags, target_user.name);

        std::string seclevel_string = "Seclevel: ";

        int seclevel = seclevel_fraction_to_seclevel(structure.calculate_seclevel());
        std::string sstring = seclevel_to_string(seclevel);

        std::string col = string_to_colour(sstring);

        seclevel_string += "(`" + col + to_string_with_enforced_variable_dp(structure.calculate_seclevel(), 2) + "` - `" + col + sstring + "`)";

        result = "Current Sys: " + colour_string(structure.name) + "\n" + seclevel_string + "\n" + result;

        return js::make_value(vctx, result);
    }
    else
    {
        std::vector<nlohmann::json> all_npc_data;

        for(auto& i : info.ring_ordered_names)
        {
            const std::string& name = i;
            vec3f pos = info.global_pos[name];

            nlohmann::json j;
            j["name"] = name;
            j["x"] = pos.x();
            j["y"] = pos.y();
            j["z"] = pos.z();

            auto connections = playspace_network_manage.get_links(name);

            for(auto it = connections.begin(); it != connections.end();)
            {
                if(info.accessible.find(*it) == info.accessible.end())
                    it = connections.erase(it);
                else
                    it++;
            }

            std::vector<float> stabs;

            for(int i=0; i < (int)connections.size(); i++)
            {
                auto val = playspace_network_manage.get_neighbour_link_strength(name, connections[i]);

                if(val.has_value())
                    stabs.push_back(val.value());
                else
                    stabs.push_back(-1.f);
            }


            j["links"] = connections;
            j["stabilities"] = stabs;

            bool is_special = std::find_if(special_users.begin(), special_users.end(), [&](user& u1){return u1.name == name;}) != special_users.end();

            if(is_special)
            {
                j["is_long_distance_traveller"] = true;
                j["connected_systems"] = structure.get_connected_systems_from(name);
            }

            all_npc_data.push_back(j);
        }

        nlohmann::json final_data = all_npc_data;

        return js::make_value(vctx, final_data);
    }

    ///to go any further we need 2 things
    ///one: how to represent the glob of npcs on the map
    ///two: the system connecting npcs (spoilers)

    ///alright ok
    ///the game design can kind of get saved easily here
    ///so we insert the special npcs into the system
    ///connect them to the regular npc net
    ///and that's the entrance
    ///hooray!

    return js::make_value(vctx, "Unreachable");
}

duk_ret_t sys__move(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    bool has_to = dukx_is_prop_truthy(ctx, -1, "to");
    bool has_confirm = dukx_is_prop_truthy(ctx, -1, "confirm");
    bool has_stop = dukx_is_prop_truthy(ctx, -1, "stop");
    bool has_queue = dukx_is_prop_truthy(ctx, -1, "queue");
    bool has_array = dukx_is_prop_truthy(ctx, -1, "array");
    std::optional<user> my_user_opt = get_user(get_caller(ctx), get_thread_id(ctx));

    double fraction = 1.f;

    if(dukx_has_prop_string(ctx, -1, "fraction"))
        fraction = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "fraction", 1);
    if(dukx_has_prop_string(ctx, -1, "frac"))
        fraction = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "frac", 1);
    if(dukx_has_prop_string(ctx, -1, "f"))
        fraction = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "f", 1);

    double move_offset = 0.f;

    if(dukx_has_prop_string(ctx, -1, "offset"))
        move_offset = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "offset", 1);
    if(dukx_has_prop_string(ctx, -1, "o"))
        move_offset = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "o", 1);

    if(!my_user_opt.has_value())
        return push_error(ctx, "No User, really bad error");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();
    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    bool is_anchored = playspace_network_manage.current_network_links(get_caller(ctx)) > 0;

    user& my_user = *my_user_opt;

    if(has_stop && !is_anchored)
    {
        my_user.set_local_pos(my_user.get_local_pos());

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

            my_user.overwrite_user_in_db(mongo_ctx);
        }

        push_duk_val(ctx, "Stopped");
        return 1;
    }

    ///needs to return if we're anchored or not
    ///maybe we have the concept of dropping anchor, which connects us into the network
    if(!has_to)
    {
        space_pos_t pos = my_user.get_local_pos();

        std::string str = "x: " + to_string_with_enforced_variable_dp(pos.x(), 2) +
                          " | y: " + to_string_with_enforced_variable_dp(pos.y(), 2) +
                          " | z: " + to_string_with_enforced_variable_dp(pos.z(), 2);

        std::string msg = "Please specify " + make_key_val("to", "\"user\"") + " or " +
                                              //make_key_val("to", "\"system\"") + ", or " +
                                              make_key_val("to", "[x, y, z]");


        std::vector<nlohmann::json> found_json;

        nlohmann::json all_json;

        size_t current_time = get_wall_time();

        {
            nlohmann::json j;
            j["x"] = pos.x();
            j["y"] = pos.y();
            j["z"] = pos.z();
            j["timestamp_ms"] = current_time;

            all_json["current"] = j;
        }

        std::string remaining_string;

        timestamp_move_queue my_queue = my_user.get_timestamp_queue();
        my_queue.cleanup_old_elements(current_time);

        for(int i=0; i < (int)my_queue.timestamp_queue.size(); i++)
        {
            timestamped_position& position = my_queue.timestamp_queue[i];

            if(current_time >= position.timestamp)
                continue;

            nlohmann::json j;

            if(position.is_move())
            {
                remaining_string += "Position: " +
                                                to_string_with_enforced_variable_dp(position.position.x(), 2) + " " +
                                                to_string_with_enforced_variable_dp(position.position.y(), 2) + " " +
                                                to_string_with_enforced_variable_dp(position.position.z(), 2) + " in " +
                                                to_string_with_enforced_variable_dp((position.timestamp - current_time) / 1000., 2) + + "s";

                j["type"] = "move";

                j["x"] = position.position.x();
                j["y"] = position.position.y();
                j["z"] = position.position.z();

                j["timestamp_ms"] = position.timestamp;
                j["finish_in_ms"] = position.timestamp - current_time;

                found_json.push_back(j);
            }

            if(position.is_activate())
            {
                remaining_string += "Move to System: " + position.system_to_arrive_at;

                j["type"] = "activate";
                j["system_to_arrive_at"] = position.system_to_arrive_at;

                found_json.push_back(j);
            }

            if(i != (int)my_queue.timestamp_queue.size()-1)
                remaining_string += "\n";
        }

        if(remaining_string != "")
        {
            remaining_string = "\nMove Queue:\n" + remaining_string;
        }

        all_json["queue"] = found_json;

        if(!has_array)
        {
            push_duk_val(ctx, str + "\n" + msg + remaining_string);
        }
        else
        {
            push_duk_val(ctx, all_json);
        }

        return 1;
    }
    else
    {
        if(is_anchored && !has_confirm)
        {
            std::string str = "Please " + make_key_val("confirm", "true") + " to disconnect from the network";

            push_duk_val(ctx, str);
            return 1;
        }

        std::string total_msg = "";

        if(is_anchored && has_confirm)
        {
            total_msg += "Disconnected\n";

            playspace_network_manage.unlink_all(my_user.name);
        }


        vec3f current_pos;

        if(!has_queue)
            current_pos = my_user.get_local_pos();
        else
            current_pos = my_user.get_final_pos().position;

        vec3f end_pos;

        ///should really cancel the last move that was made and then make queue optional
        duk_get_prop_string(ctx, -1, "to");

        if(duk_is_array(ctx, -1))
        {
            duk_pop(ctx);

            ///perform move
            std::vector<double> values = dukx_get_prop_as<std::vector<double>>(ctx, -1, "to");

            if(values.size() == 2)
                values.push_back(0);

            if(values.size() != 3)
            {
                push_duk_val(ctx, total_msg + "Requires [x, y] or [x, y, z]");
                return 1;
            }

            for(auto& i : values)
            {
                if(!isfinite(i))
                {
                    return push_error(ctx, "Values must be finite");
                }
            }

            end_pos = {values[0], values[1], values[2]};
        }
        else if(duk_is_string(ctx, -1))
        {
            duk_pop(ctx);

            std::string str = dukx_get_prop_as<std::string>(ctx, -1, "to");

            user targeting_user;

            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

                if(!targeting_user.load_from_db(mongo_ctx, str))
                    return push_error(ctx, "Invalid user");
            }

            if(!low_level_structure_manage.in_same_system(targeting_user, my_user))
                return push_error(ctx, "Not in the current system");

            end_pos = targeting_user.get_local_pos();

            vec3f diff = current_pos - end_pos;

            if(diff.length() > MINSEP)
            {
                end_pos += diff.norm() * MINSEP * 2;
            }
        }
        else
        {
            duk_pop(ctx);

            return push_error(ctx, "Requires string or array");
        }

        for(int i=0; i < 3; i++)
        {
            if(!isfinite(end_pos.v[i]))
                return push_error(ctx, "Not a finite value");
        }

        end_pos = clamp(end_pos, -1000.f, 1000.f);

        end_pos = (end_pos - current_pos) * fraction + current_pos;
        vec3f diff = (end_pos - current_pos).norm();

        ///move away from end pos
        end_pos = end_pos - diff * move_offset;

        if(!has_queue)
        {
            my_user.set_local_pos(my_user.get_local_pos());
        }

        double units_per_second = 2;

        vec3f distance;

        if(!has_queue)
        {
            distance = (end_pos - my_user.get_local_pos());
        }
        else
        {
            distance = (end_pos - my_user.get_final_pos().position);
        }

        double linear_distance = distance.length();

        double time_to_travel_distance_s = linear_distance / units_per_second;

        size_t travel_offset = time_to_travel_distance_s * 1000;

        timestamped_position move_to;

        move_to.timestamp = travel_offset;
        move_to.position = end_pos;

        my_user.add_position_target(move_to.position, move_to.timestamp, make_success_col("-Move Completed-"));

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

            my_user.overwrite_user_in_db(mongo_ctx);
        }

        return push_success(ctx, "Travel time of " + to_string_with_enforced_variable_dp(time_to_travel_distance_s, 2) + "s");
    }

    return push_error(ctx, "Impossible");
}

std::string price_to_string(int price)
{
    if(price == 0)
        return "[" + make_success_col("free") + "]";

    return "[" + std::to_string(price) + "]";
}

///so this function is acquiring a few if(not_user){do_some_display_thing}
///aka a bunch of conditional logic which is really just display logic
///what should do instead is build the logic for everything separately then merge together stuff
///that actually is to be displayed at the end
js::value sys__access(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl)
{
    #ifdef TESTING
    /*MAKE_PERF_COUNTER();
    mongo_diagnostics diagnostic_scope;*/
    #endif // TESTING

    if(!arg["user"].is_truthy())
        return js::make_error(vctx, "Takes a user parameter");

    std::string target_name = arg["user"];
    bool has_activate = arg["activate"].is_truthy();
    bool has_queue = arg["queue"].is_truthy();
    bool has_connect = arg["connect"].is_truthy();
    bool has_disconnect = arg["disconnect"].is_truthy();
    bool has_modify = arg["modify"].is_truthy();
    bool has_confirm = arg["confirm"].is_truthy();
    bool has_arr = requested_scripting_api(arg);
    bool has_users = arg["users"].is_truthy();

    std::string add_user = arg["add"];
    std::string remove_user = arg["remove"];
    bool view_users = arg["view"].is_truthy();

    /*int n_count = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", 1);
    n_count = clamp(n_count, 1, 100);*/

    int n_count = 1;

    user target;
    user my_user;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

        if(!target.load_from_db(mongo_ctx, target_name))
            return js::make_error(vctx, "Invalid user");

        if(!my_user.load_from_db(mongo_ctx, get_caller(vctx)))
            return js::make_error(vctx, "Invalid host, really bad");
    }

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    auto target_sys_opt = low_level_structure_manage.get_system_of(target.name);
    auto my_sys_opt = low_level_structure_manage.get_system_of(my_user.name);

    if(!target_sys_opt.has_value() || !my_sys_opt.has_value())
        return js::make_error(vctx, "Well then you are lost (high ground!)");

    if(!(target.is_npc() && target.is_allowed_user(get_caller(vctx))))
    {
        if(target_sys_opt.value() != my_sys_opt.value())
            return js::make_error(vctx, "Not in the same system");

        if(!playspace_network_manage.has_accessible_path_to(vctx, target.name, my_user.name, (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
            return js::make_error(vctx, "No path");
    }

    bool in_same_system = target_sys_opt.value() == my_sys_opt.value();

    low_level_structure& current_sys = *my_sys_opt.value();
    low_level_structure& target_sys = *target_sys_opt.value();

    bool is_warpy = false;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_npc_properties_context(get_thread_id(vctx));

        is_warpy = npc_info::has_type(mongo_ctx, npc_info::WARPY, target.name);
    }

    user_nodes target_nodes = get_nodes(target.name, get_thread_id(vctx));

    auto valid_actions = target_nodes.valid_hostile_actions();
    bool can_modify_users = (target.is_allowed_user(get_caller(vctx)) || ((valid_actions & user_node_info::CLAIM_NPC) > 0)) && target.is_npc() && !is_warpy;

    std::string total_msg;

    space_pos_t my_local_pos = my_user.get_local_pos();

    std::string situation_string = "Location: [" +
                                        to_string_with_enforced_variable_dp(my_local_pos.x(), 2) + ", " +
                                        to_string_with_enforced_variable_dp(my_local_pos.y(), 2) + ", " +
                                        to_string_with_enforced_variable_dp(my_local_pos.z(), 2) + "]\n";

    total_msg += situation_string;

    //std::string sector_string = "Sector: " + usr.fetch_sector();
    //total_msg += sector_string;

    std::string system_string = "System: " + colour_string(current_sys.name);
    total_msg += system_string + "\n";

    double maximum_warp_distance = MAXIMUM_WARP_DISTANCE;

    float distance = (target.get_local_pos() - my_user.get_local_pos()).length();

    nlohmann::json array_data;

    array_data["distance"] = distance;

    array_data["is_long_distance_traveller"] = is_warpy;

    if(is_warpy && in_same_system)
    {
        std::vector<std::string> connected = playspace_network_manage.get_links(target.name);
        std::vector<user> connected_users = load_users_nolock(connected);

        std::string connected_system;
        low_level_structure* found_system = nullptr;
        user* destination_user = nullptr;

        for(user& usr : connected_users)
        {
            bool of_type = false;

            {
                mongo_nolock_proxy mongo_ctx = get_global_mongo_npc_properties_context(get_thread_id(vctx));

                of_type = npc_info::has_type(mongo_ctx, npc_info::WARPY, usr.name);
            }

            if(of_type)
            {
                auto connected_sys_opt = low_level_structure_manage.get_system_of(usr);

                if(!connected_sys_opt.has_value() || connected_sys_opt.value() == target_sys_opt.value())
                    continue;

                low_level_structure& structure = *connected_sys_opt.value();

                connected_system = structure.name;
                found_system = connected_sys_opt.value();
                destination_user = &usr;

                break;
            }
        }

        if(distance > maximum_warp_distance && !has_queue)
        {
            total_msg += "Target out of range to " + make_key_col("activate") + ", max range is " + to_string_with_enforced_variable_dp(maximum_warp_distance, 2) + ", found range " + to_string_with_enforced_variable_dp(distance, 2) + ". " + make_key_val("queue", "true") + "?\n";

            array_data["can_activate"] = false;
            array_data["out_of_range_to_activate"] = true;
        }
        else
        {
            if(connected_system != "" && !has_activate)
            {
                total_msg += "Please " + make_key_val("activate", "true") + " to travel to " + connected_system + ". Pass " + make_key_val("queue", "true") +" to execute after current moves are finished, instead of immediately\n";

                array_data["can_activate"] = true;
            }

            if(has_activate && found_system != nullptr)
            {
                if(has_queue)
                {
                    my_user.add_activate_target(my_user.get_final_pos().timestamp, connected_system);

                    array_data["queued_engaged"] = true;

                    total_msg += "Queued. Travelling to " + connected_system + " after current move queue is complete\n";
                }
                else
                {
                    playspace_network_manage.unlink_all(my_user.name);

                    total_msg += "Engaged. Travelling to " + connected_system + "\n";

                    found_system->steal_user(my_user, current_sys, destination_user->get_local_pos(), target.get_local_pos());

                    array_data["engaged"] = true;

                    create_notification(get_thread_id(vctx), my_user.name, make_notif_col("-Arrived at " + connected_system + "-"));
                }
            }
        }

        ///should also print sys.view map
        {
            mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

            my_user.overwrite_user_in_db(mongo_ctx);
        }
    }

    std::string links_string = "";

    {
        network_accessibility_info info = playspace_network_manage.generate_network_accessibility_from(vctx, target.name, n_count);

        std::vector<nlohmann::json> all_npc_data = get_net_view_data_arr(info);

        //array_data["links"] = all_npc_data;

        const std::string& name = target_name;
        vec3f pos = info.global_pos[name];

        array_data["name"] = name;
        array_data["x"] = pos.x();
        array_data["y"] = pos.y();
        array_data["z"] = pos.z();

        auto connections = playspace_network_manage.get_links(name);

        for(auto it = connections.begin(); it != connections.end();)
        {
            if(info.accessible.find(*it) == info.accessible.end())
                it = connections.erase(it);
            else
                it++;
        }

        std::vector<float> stabs;

        for(int i=0; i < (int)connections.size(); i++)
        {
            auto val = playspace_network_manage.get_neighbour_link_strength(name, connections[i]);

            if(val.has_value())
                stabs.push_back(val.value());
            else
                stabs.push_back(-1.f);
        }


        array_data["links"] = connections;
        array_data["stabilities"] = stabs;

        links_string = get_net_view_data_str(all_npc_data, false);
    }

    ///handle connection
    ///should be able to connect to a visible npc
    ///we need a more general way to define this visibility rule as its not uncommon now
    ///maybe should handle this in playspace_network_manager?
    if(!is_warpy)
    {
        std::string connections = "Target Links: " + std::to_string(playspace_network_manage.current_network_links(target.name)) + "/" + std::to_string(playspace_network_manage.max_network_links(target.name))
                                //+ "\n";
                                + " " + links_string + "\n";

        total_msg += connections;

        array_data["max_links"] = playspace_network_manage.max_network_links(target.name);

        ///leaving the debugging here because inevitably itll be necessary
        /*auto flinks = playspace_network_manage.get_links(target.name);

        for(auto& i : flinks)
        {
            std::cout << i << std::endl;
        }*/

        if(!has_users && get_caller(vctx) != target.name && in_same_system)
        {
            if(!playspace_network_manage.is_linked(my_user.name, target.name))
            {
                bool is_valid = true;

                //if(playspace_network_manage.current_network_links(my_user.name) == 0)
                {
                    vec3f current_local = my_user.get_local_pos();
                    vec3f target_local = target.get_local_pos();

                    vec3f diff = current_local - target_local;

                    float length = diff.length();

                    if(length > ESEP * 2)
                    {
                        is_valid = false;
                        total_msg += make_error_col("Out of Range") + " to " + make_key_col("connect") + "\n";

                        array_data["out_of_range"] = true;
                    }

                    if(length < MINSEP)
                    {
                        is_valid = false;
                        total_msg += make_error_col("Too close ") + " to " + make_key_col("connect") + "\n";

                        array_data["too_close"] = true;
                    }
                }

                if(playspace_network_manage.current_network_links(target.name) < playspace_network_manage.max_network_links(target.name) &&
                   playspace_network_manage.current_network_links(my_user.name) < playspace_network_manage.max_network_links(my_user.name) && is_valid)
                {
                    if(!has_connect)
                    {
                        total_msg += "Pass " + make_key_val("connect", "true") + " to attempt a connection\n";

                        array_data["can_connect"] = true;
                    }
                    else
                    {
                        ///not being used yet
                        total_msg + make_success_col("Linked " + my_user.name + " to " + target.name) + "\n";

                        array_data["linked"] = true;

                        ///code for moving stuff around in a hypothetical impl
                        #if 0
                        if(playspace_network_manage.current_network_links(my_user.name) == 0)
                        {
                            vec3f current_local = my_user.get_local_pos();
                            vec3f target_local = target.get_local_pos();

                            vec3f relative = (current_local - target_local).norm();
                            vec3f absolute = relative * ESEP + target_local;

                            my_user.set_local_pos(absolute);

                            ///would need to flush to db here
                        }
                        #endif // 0

                        my_user.set_local_pos(my_user.get_local_pos());

                        {
                            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

                            my_user.overwrite_user_in_db(mongo_ctx);
                        }

                        return create_and_modify_link(vctx, my_user.name, my_user.name, target.name, true, 15.f, has_confirm, true);
                    }
                }
            }
            else
            {
                if(!has_disconnect)
                {
                    total_msg += "Pass " + make_key_val("disconnect", "true") + " to disconnect from this target\n";

                    array_data["can_disconnect"] = true;
                }

                if(has_disconnect && !has_confirm)
                {
                    total_msg += "Please " + make_key_val("confirm", "true") + " to confirm disconnection\n";

                    array_data["confirm_disconnect"] = true;
                }

                if(has_disconnect && has_confirm)
                {
                    total_msg += make_error_col("Disconnected " + my_user.name + " from " + target.name) + "\n";

                    playspace_network_manage.unlink(my_user.name, target.name);

                    array_data["disconnected"] = true;
                }
            }
        }
    }

    if(!has_users && in_same_system && target.name != my_user.name)
    {
        ///must have a path for us to be able to access the npc
        if(!has_modify)
        {
            total_msg += "Pass " + make_key_val("modify", "num") + " to strengthen or weaken the path to this target\n";

            array_data["can_modify_links"] = true;
        }
        else
        {
            double amount = arg["modify"];

            if(amount == 0)
            {
                total_msg += "Modify should be greater or less than 0\n";
            }
            else
            {
                return create_and_modify_link(vctx, my_user.name, my_user.name, target.name, false, amount, has_confirm, true);
            }
        }
    }

    array_data["can_modify_users"] = can_modify_users;

    if(can_modify_users)
    {
        if(has_users)
        {
            std::string str_add = "Pass " + make_key_val("add", "\"\"") + " to add a user";
            std::string str_remove = "Pass " + make_key_val("remove", "\"\"") + " to remove a user";
            std::string str_view = "Pass " + make_key_val("view", "true") + " to view allowed user list";

            double base_npc_purchase_cost = target_sys.get_npc_purchase_cost();

            int add_price = base_npc_purchase_cost;
            int remove_price = base_npc_purchase_cost / 4;
            int view_price = base_npc_purchase_cost / 40;

            if(target.is_allowed_user(get_caller(vctx)))
            {
                add_price = 0;
                remove_price = 0;
                view_price = 0;
            }

            array_data["add_price"] = add_price;
            array_data["remove_price"] = remove_price;
            array_data["view_price"] = view_price;


            total_msg += str_add + " " + price_to_string(add_price) + "\n";
            total_msg += str_remove + " " + price_to_string(remove_price) + "\n";
            total_msg += str_view + " " + price_to_string(view_price) + "\n";

            std::vector<std::string> allowed_users = target.get_allowed_users();

            if(view_users)
            {
                if(!target.is_allowed_user(get_caller(vctx)))
                {
                    if(auto res = handle_confirmed(vctx, has_confirm, get_caller(vctx), view_price); res.has_value())
                        return res.value();
                }

                std::string ret;

                if(target.is_allowed_user(get_caller(vctx)))
                    ret += make_success_col("Authed Users");
                else
                    ret += make_error_col("Authed Users");

                ret += ": [";

                for(int i=0; i < (int)allowed_users.size(); i++)
                {
                    if(i != (int)allowed_users.size()-1)
                        ret += colour_string(allowed_users[i]) + ", ";
                    else
                        ret += colour_string(allowed_users[i]);
                }

                ret += "]\n";

                total_msg += ret;

                array_data["viewed_users"] = allowed_users;
            }

            if(add_user != "")
            {
                bool user_is_valid = false;

                {
                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

                    user_is_valid = user().load_from_db(mongo_ctx, add_user);
                }

                if(!target.is_npc())
                    return js::make_error(vctx, "Cannot take over a user");

                if(user_is_valid)
                {
                    if(!target.is_allowed_user(get_caller(vctx)))
                    {
                        if(auto res = handle_confirmed(vctx, has_confirm, get_caller(vctx), add_price); res.has_value())
                            return res.value();
                    }

                    total_msg += make_success_col("Added User") + ": " + colour_string(add_user) + "\n";

                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

                    target.add_allowed_user(add_user, mongo_ctx);
                    target.overwrite_user_in_db(mongo_ctx);

                    array_data["added_user"] = add_user;
                }
                else
                {
                    return js::make_error(vctx, "Add User is not valid (" + add_user + ")");
                }
            }

            if(remove_user != "")
            {
                bool user_is_valid = false;

                {
                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

                    user_is_valid = user().load_from_db(mongo_ctx, remove_user);
                }

                if(user_is_valid)
                {
                    if(!target.is_allowed_user(get_caller(vctx)))
                    {
                        if(auto res = handle_confirmed(vctx, has_confirm, get_caller(vctx), remove_price); res.has_value())
                            return res.value();
                    }

                    total_msg += make_success_col("Removed User") + ": " + colour_string(remove_user) + "\n";

                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(vctx));

                    target.remove_allowed_user(remove_user, mongo_ctx);
                    target.overwrite_user_in_db(mongo_ctx);

                    array_data["removed_user"] = remove_user;
                }
                else
                {
                    return js::make_error(vctx, "Remove User is not valid (" + remove_user + ")");
                }
            }
        }
        else
        {
            total_msg += "Pass " + make_key_val("users", "true") + " to perform user management\n";
        }
    }

    if(has_arr)
    {
        return js::make_value(vctx, array_data);
    }

    //total_msg += links_string;

    if(total_msg == "")
        return js::make_value(vctx, "");

    if(total_msg.size() > 0 && total_msg.back() == '\n')
        total_msg.pop_back();

    return js::make_value(vctx, total_msg);
}

duk_ret_t sys__limits(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    js::value_context vctx(ctx);

    std::string sys_name = duk_safe_get_prop_string(ctx, -1, "sys");
    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");
    std::string user_name = duk_safe_get_prop_string(ctx, -1, "user");

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    std::optional<low_level_structure*> sys_opt;
    std::optional<low_level_structure*> sys_current_opt;

    sys_current_opt = low_level_structure_manage.get_system_of(get_caller(ctx));

    if(sys_name != "")
    {
        sys_opt = low_level_structure_manage.get_system_from_name(sys_name);
    }
    else if(user_name != "")
    {
        playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

        if(!playspace_network_manage.has_accessible_path_to(vctx, user_name, get_caller(vctx), (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
            return push_error(ctx, "No path");

        sys_opt = low_level_structure_manage.get_system_of(user_name);
    }
    else
        sys_opt = sys_current_opt;

    if(!sys_opt.has_value())
        return push_error(ctx, "No such system");

    auto user_opt = get_user(get_caller(ctx), get_thread_id(ctx));

    if(!user_opt.has_value())
        return push_error(ctx, "Error no such user");

    user& usr = user_opt.value();

    size_t current_time = get_wall_time();

    low_level_structure& sys = *sys_opt.value();
    low_level_structure& sys_base = *sys_current_opt.value();

    double cash_send = sys.get_ratelimit_max_cash_send();
    double cash_steal_percent = sys.get_ratelimit_max_cash_percentage_steal();
    double item_send = sys.get_ratelimit_max_item_send();
    double item_steal = sys.get_ratelimit_max_item_steal();

    double current_cash_send = usr.get_max_sendable_cash(current_time, sys, sys_base);
    double current_item_send = usr.get_max_sendable_items(current_time, sys, sys_base);

    double seclevel_1 = sys.calculate_seclevel();
    double seclevel_2 = sys_base.calculate_seclevel();

    double current_cash_stealable = usr.get_max_stealable_cash(current_time, sys);
    double current_items_stealable = usr.get_max_stealable_items(current_time, sys);

    double charge_time_ms = user_limit::recharge_time_ms;
    double charge_time_h = charge_time_ms / 1000. / 60. / 60.;

    //std::cout << cash_steal_percent * usr.get_pvp_old_cash_estimate(current_time) << std::endl;

    ///show how much cash can be stolen from me as well

    ///needs to have +inf for within system xfers?
    ///might be exploitable

    std::string extra_args = "Pass " + make_key_val("sys", "\"example\"") + " to view a specific system's limits\n";
    extra_args += "Pass " + make_key_val("user", "\"username\"") + " to see limits to a specific user\n";

    if(!is_arr)
    {
        std::string rstr = extra_args;

        //rstr += "Limits due to Security Levels:\n";
        rstr += "System: " + colour_string(sys_base.name) + " (`" + seclevel_fraction_to_colour(seclevel_2) + to_string_with_enforced_variable_dp(seclevel_2, 2) + "`)\n";

        if(user_name != "")
        {
            rstr += "Target: " + colour_string(user_name) + "\n";
        }
        else if(sys_opt.has_value() && sys_name != "")
        {
            rstr += "Target: " + colour_string(sys.name) + " (`" + seclevel_fraction_to_colour(seclevel_1) + to_string_with_enforced_variable_dp(seclevel_1, 2) + "`)\n";
        }

        /*rstr += "Sendable Cash (passive): " + to_string_with_enforced_variable_dp(cash_send, 2) + "\n";
        rstr += "Sendable Items (passive): " + std::to_string((int)item_send) + "\n";

        rstr += "Stealable Cash % (passive): " + to_string_with_enforced_variable_dp(cash_steal_percent * 100., 2) + "\n";
        rstr += "Stealable Items (passive): " + std::to_string((int)item_steal) + "\n";

        rstr += "Current Cash Sendable: " + to_string_with_enforced_variable_dp(current_cash_send, 2) + "\n";
        rstr += "Current Items Sendable: " + std::to_string((int)current_item_send) + "\n";

        rstr += "Current Cash Stealable from you: " + to_string_with_enforced_variable_dp(current_cash_stealable, 2) + "\n";
        rstr += "Current Items Stealable from you: " + std::to_string((int)current_items_stealable);*/

        rstr += make_cash_col("Cash") + ":\n";
        rstr += "    " + make_success_col("Sendable") + ":\n";
        rstr += "        Max:" + to_string_with_enforced_variable_dp(cash_send, 2) + "\n";
        rstr += "        Cur:" + to_string_with_enforced_variable_dp(current_cash_send, 2) + "\n";
        rstr += "    " + make_error_col("Stealable") + ": (from " + colour_string(get_caller(ctx)) + ")\n";
        rstr += "        Max:" + to_string_with_enforced_variable_dp(cash_steal_percent*100, 2) + "%" + "\n";
        rstr += "        Cur:" + to_string_with_enforced_variable_dp(current_cash_stealable, 2) + "\n";

        rstr += make_item_col("Item") + ":\n";
        rstr += "    " + make_success_col("Sendable") + ":\n";
        rstr += "        Max:" + std::to_string((int)item_send) + "\n";
        rstr += "        Cur:" + std::to_string((int)current_item_send) + "\n";
        rstr += "    " + make_error_col("Stealable") + ": (from " + colour_string(get_caller(ctx)) + ")\n";
        rstr += "        Max:" + std::to_string((int)item_steal) + "\n";
        rstr += "        Cur:" + std::to_string((int)current_items_stealable) + "\n";

        rstr += "\nLimits take " + std::to_string((int)charge_time_h) + "h to refill fully";

        push_duk_val(ctx, rstr);
    }
    else
    {
        nlohmann::json ret;
        ret["current_system"] = sys_base.name;
        ret["current_seclevel"] = seclevel_2;

        if(user_name != "")
        {
            ret["target_user"] = user_name;
        }
        else if(sys_opt.has_value() && sys_name != "")
        {
            ret["target_system"] = sys_name;
            ret["target_seclevel"] = seclevel_1;
        }

        ret["max_cash_send"] = cash_send;
        ret["current_cash_send"] = current_cash_send;

        ret["max_cash_steal_fraction"] = cash_steal_percent;
        ret["current_cash_steal"] = current_cash_stealable;

        ret["max_item_send"] = item_send;
        ret["current_item_send"] = current_item_send;

        ret["max_item_steal"] = item_steal;
        ret["current_item_steal"] = current_items_stealable;

        ret["refill_time_ms"] = charge_time_ms;

        push_duk_val(ctx, ret);
    }

    return 1;
}

duk_ret_t sys__debug(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string str = duk_safe_get_prop_string(ctx, -1, "sys");

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    user my_user;

    {
        mongo_lock_proxy lock = get_global_mongo_user_info_context(-2);

        if(!my_user.load_from_db(lock, get_caller(ctx)))
            return push_error(ctx, "Error: Does not exist");
    }

    std::optional<low_level_structure*> opt_structure;

    if(str == "")
        opt_structure = low_level_structure_manage.get_system_of(my_user);
    else
        opt_structure = low_level_structure_manage.get_system_from_name(str);

    if(!opt_structure.has_value())
        return push_error(ctx, "You are lost, there is no help for you now");

    low_level_structure& found_structure = *opt_structure.value();

    found_structure.layout_internal_users();

    std::vector<user> special_users = found_structure.get_special_users(get_thread_id(ctx));

    std::vector<std::vector<std::string>> buffer = ascii_make_buffer({80, 40}, false);

    js::value_context vctx(ctx);

    network_accessibility_info info;

    for(user& usr : special_users)
    {
        network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(vctx, usr.name, 15);

        info = network_accessibility_info::merge_together(info, cur);
    }

    std::string from = get_caller(ctx);

    vec3f pos = {0,0,0};
    ///info.global_pos[from]

    std::string result = ascii_render_from_accessibility_info(info, buffer, pos, 0.5f, ascii::NONE);

    push_duk_val(ctx, result);

    ///to go any further we need 2 things
    ///one: how to represent the glob of npcs on the map
    ///two: the system connecting npcs (spoilers)

    ///alright ok
    ///the game design can kind of get saved easily here
    ///so we insert the special npcs into the system
    ///connect them to the regular npc net
    ///and that's the entrance
    ///hooray!

    return 1;
}

duk_ret_t mission__list(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string caller = get_caller(ctx);

    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");

    quest_manager& quest_manage = get_global_quest_manager();

    std::vector<quest> all_quests;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_quest_manager_context(get_thread_id(ctx));
        all_quests = quest_manage.fetch_quests_of(mongo_ctx, caller);
    }


    if(is_arr)
    {
        nlohmann::json ret;

        std::vector<nlohmann::json> jsonified;

        for(quest& q : all_quests)
        {
            jsonified.push_back(q.get_as_data());
        }

        ret["quests"] = jsonified;

        push_duk_val(ctx, ret);
        return 1;
    }
    else
    {
        std::string str;

        //for(quest& q : all_quests)
        for(int idx = 0; idx < (int)all_quests.size(); idx++)
        {
            str += std::to_string(idx) + ". " + all_quests[idx].get_as_string() + "\n\n";
        }

        str = strip_trailing_newlines(str);

        if(all_quests.size() == 0)
        {
            str = "No Missions";
        }

        push_duk_val(ctx, str);
        return 1;
    }
}

duk_ret_t mission__debug(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string caller = get_caller(ctx);

    //bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");

    quest_manager& quest_manage = get_global_quest_manager();

    quest test = quest_manage.get_new_quest_for(get_caller(ctx), "Trouble in Paradise? Pt 5.", "Run #scripts.core() ayy");

    /*low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    auto sys_opt = low_level_structure_manage.get_system_of(caller);

    if(!sys_opt.has_value())
        return push_error(ctx, "No sys");

    low_level_structure& sys = *sys_opt.value();

    auto all_user_names = sys.get_all_users();

    shuffle_csprng_seed<std::minstd_rand>(all_user_names);

    auto all_users = load_users(all_user_names, get_thread_id(ctx));

    for(user& usr : all_users)
    {
        if(!usr.is_npc())
            continue;

        test.add_breach_user(usr.name);

        break;
    }*/

    test.add_run_script("scripts.core");


    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_quest_manager_context(get_thread_id(ctx));

        db_disk_overwrite(mongo_ctx, test);
    }

    test.send_new_quest_alert_to(get_thread_id(ctx), get_caller(ctx));

    return 1;

    /*std::vector<quest> all_quests;

    {
        mongo_nolock_proxy mongo_ctx = get_global_mongo_quest_manager_context(get_thread_id(ctx));
        all_quests = quest_manage.fetch_quests_of(mongo_ctx, caller);
    }


    if(is_arr)
    {
        nlohmann::json ret;

        std::vector<nlohmann::json> jsonified;

        for(quest& q : all_quests)
        {
            jsonified.push_back(q.get_as_data());
        }

        ret["quests"] = jsonified;

        push_duk_val(ctx, ret);
        return 1;
    }
    else
    {
        std::string str;

        for(quest& q : all_quests)
        {
            str += q.get_as_string() + "\n";
        }

        str = strip_trailing_newlines(str);

        if(all_quests.size() == 0)
        {
            str = "No Quests";
        }

        push_duk_val(ctx, str);
        return 1;
    }*/
}

#ifdef TESTING

duk_ret_t cheats__arm(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string target = duk_safe_get_prop_string(ctx, -1, "target");

    if(target == "")
        return push_error(ctx, "Usage: cheats.arm({target:<target>})");

    float gc_cap = 100;
    float items_cap = 10;

    distribute_loot_around(get_global_playspace_network_manager(), target, 1, 0, gc_cap, items_cap);

    return 0;

    #if 0

    std::optional opt_user_and_nodes = get_user_and_nodes(target, get_thread_id(ctx));

    if(!opt_user_and_nodes.has_value())
        return push_error(ctx, "No such user");

    std::string lock = "crt_reg";

    item test_item = item_types::get_default_of(item_types::LOCK, lock);

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(get_thread_id(ctx));
        test_item.generate_set_id(mongo_ctx);
    }

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));
        test_item.create_in_db(mongo_ctx);
    }

    if(test_item.transfer_to_user(target, get_thread_id(ctx)))
    {

    }
    else
    {
        return push_error(ctx, "Could not transfer item to caller");
    }

    opt_user_and_nodes = get_user_and_nodes(target, get_thread_id(ctx));

    std::string accum;

    //auto ret = load_item_raw(ctx, node_idx, load_idx, unload_idx, found_user, nodes, accum);

    auto ret = load_item_raw(-1, 0, -1, opt_user_and_nodes->first, opt_user_and_nodes->second, accum, get_thread_id(ctx));

    if(ret != "")
        return push_error(ctx, ret);

    push_duk_val(ctx, accum);

    return 0;
    #endif // 0
}


duk_ret_t cheats__give(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

    user u1;
    u1.load_from_db(mongo_ctx, get_caller(ctx));

    u1.cash += 200;

    u1.overwrite_user_in_db(mongo_ctx);

    return 0;
}


duk_ret_t cheats__salvage(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

    while(1){};

    return 0;
}
#endif

#ifdef LIVE_DEBUGGING

duk_ret_t cheats__debug(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    if(get_caller(ctx) != "i20k")
        return push_error(ctx, "nope");

    auto opt_user_and_nodes = get_user_and_nodes("frag_r57l7u_lrxlc", get_thread_id(ctx));

    std::cout << opt_user_and_nodes->second.nodes.size() << std::endl;

    user_nodes& nodes = opt_user_and_nodes->second;

    for(user_node& node : nodes.nodes)
    {

    }
}
#endif // LIVE_DEBUGGING

std::string sec_level_of(function_priv_t func)
{
    for(auto& i : privileged_functions)
    {
        priv_func_info inf = i.second;

        int sl = inf.sec_level;

        if(sl == 4)
            return "Fullsec";
        if(sl == 3)
            return "Highsec";
        if(sl == 2)
            return "Midsec";
        if(sl == 1)
            return "Lowsec";
        if(sl == 0)
            return "Nullsec";
    }

    return "Nullsec";
}
