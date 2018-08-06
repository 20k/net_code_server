#include "privileged_core_scripts.hpp"

#include <ratio>
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

std::map<std::string, std::vector<script_arg>> privileged_args = construct_core_args();

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

std::map<std::string, std::vector<script_arg>> construct_core_args()
{
    std::map<std::string, std::vector<script_arg>> ret;

    ret["cash.balance"] = make_cary();
    ret["cash.expose"] = make_cary("user", "\"\"");
    ret["scripts.get_level"] = make_cary("name", "\"\"");
    ret["scripts.me"] = make_cary();
    ret["scripts.public"] = make_cary();
    ret["cash.xfer_to"] = make_cary("user", "\"\"", "amount", "0");
    ret["cash.xfer_to_caller"] = make_cary();
    ret["scripts.core"] = make_cary();
    ret["msg.manage"] = make_cary();
    ret["msg.send"] = make_cary("channel", "\"0000\"", "msg", "\"\"");
    ret["msg.tell"] = make_cary("user", "\"\"", "msg", "\"\"");
    ret["msg.recent"] = make_cary("channel", "\"0000\"", "count", "99");
    ret["users.me"] = make_cary();
    ret["item.steal"] = make_cary("user", "\"\"", "idx", "0");
    ret["item.expose"] = make_cary("user", "\"\"");
    ret["item.manage"] = make_cary();
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
    ret["net.view"] = make_cary("user", "\"\"", "n", "6");
    ret["net.map"] = make_cary("user", "\"\"", "n", "6");
    //ret["net.links"] = make_cary("user", "\"\"", "n", "6");
    ret["net.hack"] = make_cary("user", "\"\"", "NID", "-1");
    ret["net.access"] = make_cary("user", "\"\"");
    ret["net.switch"] = make_cary("user", "\"\"");
    ret["net.modify"] = make_cary("user", "\"\"", "target", "\"\"");
    ret["net.move"] = make_cary("user", "\"\"", "target", "\"\"");
    ret["net.path"] = make_cary("user", "\"\"", "target", "\"\"", "min_stability", "0");
    ret["sys.view"] = make_cary("user", "\"\"", "n", "-1");
    ret["sys.map"] = make_cary("n", "-1", "centre", "false");
    ret["sys.move"] = make_cary("to", "\"\"", "queue", "false");
    ret["sys.access"] = make_cary("user", "\"\"");

    return ret;
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
                time_code_ms = std::stoll((std::string)i["time_ms"]);
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

///https://stackoverflow.com/questions/16177295/get-time-since-epoch-in-milliseconds-preferably-using-c11-chrono
///thanks stack overflow
size_t get_wall_time()
{
    size_t milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();

    return milliseconds_since_epoch;
}

double get_wall_time_s()
{
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::ratio<1, 1>> duration = now.time_since_epoch();
    double real_time = duration.count();

    return real_time;
}

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

    std::string str = duk_safe_to_std_string(ctx, -1);

    duk_pop(ctx);

    if(privileged_functions.find(str) != privileged_functions.end())
    {
        duk_push_int(ctx, privileged_functions[str].sec_level);
        return 1;
    }

    std::string script_err;

    unified_script_info script = unified_script_loading(get_thread_id(ctx), str, script_err);

    if(!script.valid)
        return push_error(ctx, script_err);

    duk_push_int(ctx, script.seclevel);

    return 1;
}


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


duk_ret_t scripts__me(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    bool make_array = dukx_is_prop_truthy(ctx, -1, "array");

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

duk_ret_t scripts__public(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    int pretty = !dukx_is_prop_truthy(ctx, -1, "array");
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

duk_ret_t cash_internal_xfer(duk_context* ctx, const std::string& from, const std::string& to, double amount)
{
    COOPERATE_KILL();

    if(amount < 0 || amount >= pow(2, 32))
    {
        push_error(ctx, "Amount error");
        return 1;
    }

    ///this is considered a catastrophically large amount
    double cash_to_destroy_link = 10000;

    if(from == to)
    {
        push_error(ctx, "Money definitely shifted hands");
        return 1;
    }

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    std::vector<std::string> path = playspace_network_manage.get_accessible_path_to(ctx, to, from, (path_info::path_info)(path_info::NONE | path_info::ALLOW_WARP_BOUNDARY), -1, amount / cash_to_destroy_link);

    if(path.size() == 0)
        return push_error(ctx, "User does not exist or is disconnected");

    std::string leak_msg = "Xfer'd " + std::to_string(amount);

    playspace_network_manage.modify_path_per_link_strength_with_logs(path, -amount / cash_to_destroy_link, {leak_msg}, get_thread_id(ctx));

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

    create_xfer_notif(ctx, from, to, amount);

    push_success(ctx);

    return 1;
}

///TODO: TRANSACTION HISTORY

duk_ret_t cash__xfer_to(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    RATELIMIT_DUK(CASH);

    ///need a get either or
    ///so we can support to and name
    duk_get_prop_string(ctx, -1, "user");

    if(!duk_is_string(ctx, -1))
    {
        push_error(ctx, "Call with user:\"usr\"");
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

duk_ret_t scripts__core(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int make_array = dukx_is_prop_truthy(ctx, -1, "array");

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


bool user_in_channel(mongo_lock_proxy& mongo_ctx, const std::string& username, const std::string& channel)
{
    mongo_requester request;
    request.set_prop("channel_name", channel);

    auto found = request.fetch_from_db(mongo_ctx);

    if(found.size() != 1)
        return false;

    auto channel_users = str_to_array(found[0].get_prop("user_list"));

    return array_contains(channel_users, username);
}


bool is_valid_channel_name(const std::string& in)
{
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

        user found_user;

        if(!found_user.load_from_db(mongo_ctx, get_caller(ctx)))
            return push_error(ctx, "No such user, very bad error");

        std::vector<std::string> chans = str_to_array(found_user.joined_channels);

        if(to_join != "" && !array_contains(chans, to_join))
        {
            chans.push_back(to_join);
        }

        if(to_leave != "" && array_contains(chans, to_leave))
        {
            auto it = std::find(chans.begin(), chans.end(), to_leave);

            chans.erase(it);
        }

        found_user.joined_channels = array_to_str(chans);
        found_user.overwrite_user_in_db(mongo_ctx);
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

std::vector<std::string> get_users_in_channel(mongo_lock_proxy& mongo_ctx, const std::string& channel)
{
    mongo_requester request;
    request.set_prop("channel_name", channel);

    auto found = request.fetch_from_db(mongo_ctx);

    if(found.size() != 1)
        return {};

        //return push_error(ctx, "Something real weird happened: Orange Canary");

    mongo_requester& chan = found[0];

    std::vector<std::string> users = str_to_array(chan.get_prop("user_list"));

    return users;
}

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

    user my_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!my_user.load_from_db(mongo_ctx, get_caller(ctx)))
            return push_error(ctx, "No such user");
    }

    channel = strip_whitespace(channel);

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    std::vector<std::string> users;

    {
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(ctx));

            if(!user_in_channel(mongo_ctx, get_caller(ctx), channel))
                return push_error(ctx, "User not in channel or doesn't exist");
        }

        if(channel != "local")
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(ctx));
            users = get_users_in_channel(mongo_ctx, channel);
        }
        else
        {
            std::optional<low_level_structure*> system_opt = low_level_structure_manage.get_system_of(get_caller(ctx));

            if(!system_opt.has_value())
                return push_error(ctx, "Dust is coarse and irritating and gets everywhere (no system)");

            low_level_structure& structure = *system_opt.value();

            users = structure.get_all_users();

            mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(get_thread_id(ctx));

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
            mongo_ctx.change_collection(current_user);

            size_t real_time = get_wall_time();

            nlohmann::json to_insert;
            to_insert["user"] = get_caller(ctx);
            to_insert["is_chat"] = 1;
            to_insert["msg"] = msg;
            to_insert["channel"] = channel;
            to_insert["time_ms"] = real_time;
            to_insert["processed"] = 0;

            insert_in_db(mongo_ctx, to_insert);
        }
    }

    send_async_message(ctx, handle_client_poll_json(my_user));

    return push_success(ctx);
}


duk_ret_t msg__tell(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();
    RATELIMIT_DUK(CHAT);

    std::string to = duk_safe_get_prop_string(ctx, -1, "user");
    std::string msg = duk_safe_get_prop_string(ctx, -1, "msg");

    if(to == "")
        return push_error(ctx, "Invalid user argument");

    if(msg.size() > 10000)
        return push_error(ctx, "Too long msg, 10k is max");

    if(!get_user(to, get_thread_id(ctx)))
        return push_error(ctx, "Invalid User");

    mongo_lock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(get_thread_id(ctx));
    mongo_ctx.change_collection(to);

    size_t real_time = get_wall_time();

    /*mongo_requester to_insert;
    to_insert.set_prop("user", get_caller(ctx));
    to_insert.set_prop("is_tell", 1);
    to_insert.set_prop("msg", msg);
    to_insert.set_prop("time_ms", real_time);
    to_insert.set_prop("processed", 0);

    to_insert.insert_in_db(mongo_ctx);*/

    nlohmann::json to_insert;
    to_insert["user"] = get_caller(ctx);
    to_insert["is_tell"] = 1;
    to_insert["msg"] = msg;
    to_insert["time_ms"] = real_time;
    to_insert["processed"] = 0;

    insert_in_db(mongo_ctx, to_insert);

    return push_success(ctx);
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

template<typename T>
inline
std::string to_string_with_enforced_variable_dp(T a_value, int forced_dp = 1)
{
    if(fabs(a_value) <= 0.0999999 && fabs(a_value) >= 0.0001)
        forced_dp++;

    std::string fstr = std::to_string(a_value);

    auto found = fstr.find('.');

    if(found == std::string::npos)
    {
        return fstr + ".0";
    }

    found += forced_dp + 1;

    if(found >= fstr.size())
        return fstr;

    fstr.resize(found);

    return fstr;
}

void create_xfer_notif(duk_context* ctx, const std::string& xfer_from, const std::string& xfer_to, double amount)
{
    COOPERATE_KILL();

    if(xfer_from == "" || xfer_to == "")
        return;

    std::string notif_from = make_notif_col("-Sent " + to_string_with_enforced_variable_dp(amount, 2) + " (xfer)-");
    std::string notif_to = make_notif_col("-Received " + to_string_with_enforced_variable_dp(amount, 2) + " (xfer)-");

    create_notification(get_thread_id(ctx), xfer_from, notif_from);
    create_notification(get_thread_id(ctx), xfer_to, notif_to);
}

void create_xfer_item_notif(duk_context* ctx, const std::string& xfer_from, const std::string& xfer_to, const std::string& item_name)
{
    COOPERATE_KILL();

    if(xfer_from == "" || xfer_to == "")
        return;

    std::string notif_from = make_notif_col("-Lost " + item_name + " (xfer)-");
    std::string notif_to = make_notif_col("-Received " + item_name + " (xfer)-");

    create_notification(get_thread_id(ctx), xfer_from, notif_from);
    create_notification(get_thread_id(ctx), xfer_to, notif_to);
}

void create_destroy_item_notif(duk_context* ctx, const std::string& to, const std::string& item_name)
{
    COOPERATE_KILL();

    if(item_name == "")
        return;

    std::string cull_msg = make_notif_col("-Destroyed " + item_name + "-");

    create_notification(get_thread_id(ctx), to, cull_msg);
}

std::string format_time(const std::string& in)
{
    if(in.size() == 1)
        return "0" + in;

    if(in.size() == 0)
        return "00";

    return in;
}

duk_ret_t msg__recent(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string channel = duk_safe_get_prop_string(ctx, -1, "channel");
    int num = duk_get_prop_string_as_int(ctx, -1, "count");
    bool pretty = !dukx_is_prop_truthy(ctx, -1, "array");
    bool is_tell = dukx_is_prop_truthy(ctx, -1, "tell");

    if(num <= 0)
        num = 10;

    if(channel.size() == 0)
        channel = "0000";

    if(num >= 100)
    {
        return push_error(ctx, "Count cannot be >= than 100");
    }

    std::cout << "fchannel " << channel << std::endl;

    if(!is_tell)
    {
        if(!is_valid_channel_name(channel))
            return push_error(ctx, "Invalid channel name");

        if(channel == "" || channel.size() >= 10)
        {
            push_error(ctx, "Usage: #ms.msg.recent({channel:\"<name>\", count:num, pretty:1})");
            return 1;
        }
    }

    if(channel.size() > 50)
        channel.resize(50);

    mongo_lock_proxy mongo_ctx = get_global_mongo_pending_notifs_context(get_thread_id(ctx));
    mongo_ctx.change_collection(get_caller(ctx));

    ///ALARM: ALARM: RATE LIMIT

    //mongo_requester request;

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

    std::cout << "FETCH OPT " << opt.dump() << std::endl;

    //std::vector<mongo_requester> found = request.fetch_from_db(mongo_ctx);

    //std::vector<std::string> json_found = mongo_ctx->find_json(get_caller(ctx), request.dump(), opt.dump());

    std::vector<nlohmann::json> found = fetch_from_db(mongo_ctx, request, opt);

    //std::cout << "found size " << found.size() << std::endl;

    /*for(auto& i : json_found)
    {
        nlohmann::json j = nlohmann::json::parse(i);

        found.push_back(j);
    }*/

    if(!pretty)
    {
        duk_push_array(ctx);

        int cur_count = 0;
        for(nlohmann::json& i : found)
        {
            duk_push_object(ctx);

            for(auto it = i.begin(); it != i.end(); it++)
            {
                if(it.key() == "_id")
                    continue;

                put_duk_keyvalue(ctx, (std::string)it.key(), (std::string)it.value());
            }

            /*for(auto& kk : i.properties)
            {
                std::string key = kk.first;
                std::string value = kk.second;

                put_duk_keyvalue(ctx, key, value);
            }*/

            duk_put_prop_index(ctx, -2, cur_count);

            cur_count++;
        }
    }
    else
    {
        std::string str = prettify_chat_strings(found, !is_tell);

        push_duk_val(ctx, str);
    }

    return 1;
}


duk_ret_t users__me(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int pretty = !dukx_is_prop_truthy(ctx, -1, "array");

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

        if(nodes.any_contains_lock(i.get_prop("item_id")))
        {
            str += " [on_node]";
        }

        return str;
    }

    std::string ret = "{\n";

    bool is_open_source = i.get_prop_as_integer("open_source");

    //for(auto& p : i.data)
    for(auto it = i.data.begin(); it != i.data.end(); it++)
    {
        if(it.key() == "_id")
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

    if(usr.has_loaded_item(i.get_prop("item_id")))
        ret += "    loaded: true\n";

    if(nodes.any_contains_lock(i.get_prop("item_id")))
        ret += "    on_node: true";

    return ret + "}";
}


nlohmann::json get_item_raw(item& i, bool is_short, user& usr, user_nodes& nodes)
{
    nlohmann::json obj;

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
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

        if(!next.exists(item_ctx, which))
            return "Something weird happened";

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
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(thread_id);

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
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

            item lock;
            lock.load_from_db(item_ctx, to_load);
            lock.breach();
            lock.overwrite_in_db(item_ctx);

            nodes.load_lock_to_any(item_ctx, to_load);
        }

        accum += "Loaded\n";
    }

    if(which == to_load && node_idx != -1)
    {
        user_node* node = nodes.type_to_node((user_node_t)node_idx);

        {
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

            if(node->can_load_lock(item_ctx, to_load))
            {
                item lock;
                lock.load_from_db(item_ctx, to_load);
                lock.breach();
                lock.overwrite_in_db(item_ctx);

                node->load_lock(to_load);
            }
        }
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
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(thread_id);
        nodes.overwrite_in_db(node_ctx);
    }

    return "";
}


void push_internal_items_view(duk_context* ctx, int pretty, int full, user_nodes& nodes, user& found_user, std::string preamble)
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

        push_duk_val(ctx, preamble + formatted);
    }
    else
    {
        std::vector<nlohmann::json> objs;

        for(std::string& item_id : to_ret)
        {
            item next;
            next.load_from_db(mongo_ctx, item_id);

            objs.push_back(get_item_raw(next, !full, found_user, nodes));
        }

        push_duk_val(ctx, objs);
    }
}


duk_ret_t item__cull(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    int idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);

    if(idx < 0)
        return push_error(ctx, "Idx out of range");

    auto opt_user_and_nodes = get_user_and_nodes(get_caller(ctx), get_thread_id(ctx));

    if(!opt_user_and_nodes.has_value())
        return push_error(ctx, "Catastrophic error Blue Walrus in item.cull");

    user& usr = opt_user_and_nodes->first;
    std::string id = usr.index_to_item(idx);

    if(id == "")
        return push_error(ctx, "No such item");

    std::string accum;

    auto ret = load_item_raw(-1, -1, idx, opt_user_and_nodes->first, opt_user_and_nodes->second, accum, get_thread_id(ctx));

    if(ret != "")
        return push_error(ctx, ret);


    {
        mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

        item found;
        found.load_from_db(items_ctx, id);

        create_destroy_item_notif(ctx, get_caller(ctx), found.get_prop("short_name"));

        found.remove_from_db(items_ctx);
    }

    usr.remove_item(id);

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        usr.overwrite_user_in_db(user_ctx);
    }

    return push_success(ctx, "Success");
}


duk_ret_t item__manage(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int pretty = !dukx_is_prop_truthy(ctx, -1, "array");
    int full = dukx_is_prop_truthy(ctx, -1, "full");

    int load_idx = duk_get_prop_string_as_int(ctx, -1, "load", -1);
    int unload_idx = duk_get_prop_string_as_int(ctx, -1, "unload", -1);
    int node_idx = duk_get_prop_string_as_int(ctx, -1, "node", -1);

    std::string node_name = duk_safe_get_prop_string(ctx, -1, "node");

    if(load_idx >= 0 && unload_idx >= 0)
        return push_error(ctx, "Only one load/unload at a time");

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
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!found_user.load_from_db(mongo_ctx, get_caller(ctx)))
            return push_error(ctx, "No such user/really catastrophic error");
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.ensure_exists(node_ctx, get_caller(ctx));
        nodes.load_from_db(node_ctx, get_caller(ctx));
    }

    if(load_idx >= 0 || unload_idx >= 0)
    {
        std::string accum;

        auto ret = load_item_raw(node_idx, load_idx, unload_idx, found_user, nodes, accum, get_thread_id(ctx));

        if(ret != "")
            return push_error(ctx, ret);

        push_duk_val(ctx, accum);
        return 1;
    }

    push_internal_items_view(ctx, pretty, full, nodes, found_user, usage + "\n");

    return 1;
}


duk_ret_t push_xfer_item_with_logs(duk_context* ctx, int item_idx, const std::string& from, const std::string& to)
{
    ///TODO: Implement and test this below here when i'm less tired
    /*std::string accum;
    auto ret = load_item_raw(-1, -1, item_idx, found_user, nodes, accum, get_thread_id(ctx));

    if(ret != "")
        return push_error(ctx, ret);*/

    if(from == to)
        return push_success(ctx, "Item definitely transferred to a different user");

    float items_to_destroy_link = 100;

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    std::vector<std::string> path = playspace_network_manage.get_accessible_path_to(ctx, to, from, (path_info::path_info)(path_info::NONE | path_info::ALLOW_WARP_BOUNDARY), -1, 1.f / items_to_destroy_link);

    if(path.size() == 0)
        return push_error(ctx, "User does not exist or is disconnected");

    item placeholder;

    if(placeholder.transfer_from_to_by_index(item_idx, from, to, get_thread_id(ctx)))
    {
        playspace_network_manage.modify_path_per_link_strength_with_logs(path, -1.f / items_to_destroy_link, {"Xfer'd Item"}, get_thread_id(ctx));

        std::string xfer = "`NItem xfer` | from: " + from  + ", to: " + to + ", index: " + std::to_string(item_idx);

        make_logs_on(ctx, from, user_node_info::ITEM_LOG, {xfer});
        make_logs_on(ctx, to, user_node_info::ITEM_LOG, {xfer});

        //duk_push_int(ctx, placeholder.get_prop_as_integer("item_id"));

        create_xfer_item_notif(ctx, from, to, placeholder.get_prop("short_name"));

        push_success(ctx);
    }
    else
        push_error(ctx, "Could not xfer");

    return 1;
}


duk_ret_t item__xfer_to(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);

    if(item_idx < 0)
    {
        push_error(ctx, "Invalid index");
        return 1;
    }

    std::string from = get_caller(ctx);
    std::string to = duk_safe_get_prop_string(ctx, -1, "user");

    {
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

        user_nodes nodes;

        {
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

            nodes.ensure_exists(node_ctx, get_caller(ctx));
            nodes.load_from_db(node_ctx, get_caller(ctx));
        }


        std::string accum;

        auto ret = load_item_raw(-1, -1, item_idx, found_user, nodes, accum, get_thread_id(ctx));

        if(ret != "")
            return push_error(ctx, ret);
    }

    push_xfer_item_with_logs(ctx, item_idx, from, to);

    return 1;
}



duk_ret_t item__bundle_script(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);
    std::string scriptname = duk_safe_get_prop_string(ctx, -1, "name");
    std::string tag = duk_safe_get_prop_string(ctx, -1, "tag");

    if(tag.size() > 8)
        return push_error(ctx, "Tag must be <= 8 characters");

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

        if(!found_bundle.exists(item_lock, item_id))
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


        std::string name = found_bundle.get_prop("short_name");

        if(tag != "")
        {
            name += " [" + tag + "]";

            found_bundle.set_prop("short_name", name);
        }

        found_script.fill_as_bundle_compatible_item(found_bundle);
        found_bundle.set_prop("full", 1);

        found_bundle.overwrite_in_db(item_lock);
    }

    return push_success(ctx);
}


duk_ret_t item__register_bundle(priv_context& priv_ctx, duk_context* ctx, int sl)
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

        if(!found_bundle.exists(item_lock, item_id))
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

duk_ret_t item__configure_on_breach(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);
    std::string scriptname = duk_safe_get_prop_string(ctx, -1, "name");

    if(scriptname.size() > 60)
        return push_error(ctx, "Too long script name");

    bool has_name = dukx_is_prop_truthy(ctx, -1, "name");
    bool confirm = dukx_is_prop_truthy(ctx, -1, "confirm");

    if(item_idx == -1)
        return push_error(ctx, "Usage: idx:num, name:\"script_name\"");

    user usr;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!usr.load_from_db(mongo_ctx, get_caller(ctx)))
            return push_error(ctx, "Invalid calling user");
    }

    std::string item_id = usr.index_to_item(item_idx);

    if(item_id == "")
        return push_error(ctx, "Invalid item");

    item it;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

        if(!it.load_from_db(mongo_ctx, item_id))
            return push_error(ctx, "No such item");
    }

    if(it.get_prop_as_integer("item_type") != item_types::ON_BREACH)
        return push_error(ctx, "Wrong item type, must be on_breach");

    std::string new_name = on_breach_name_to_real_script_name(scriptname, priv_ctx.original_host);
    std::string real_name = on_breach_name_to_real_script_name(it.get_prop("script_name"), priv_ctx.original_host);

    if(!has_name)
        return push_success(ctx, "Configured to run as " + real_name);

    if(new_name == "")
        return push_error(ctx, "Invalid name");

    if(has_name && !confirm)
    {
        return push_success(ctx, "Please run " + make_key_col("confirm") + ":" + make_val_col("true") + " to confirm setting on_breach script name to " + new_name);
    }

    if(has_name && confirm)
    {
        it.set_prop("script_name", scriptname);

        {
             mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

             it.overwrite_in_db(mongo_ctx);
        }

        return push_success(ctx, "Set on_breach script name to " + new_name);
    }

    push_duk_val(ctx, "Should be impossible to reach here");
    return 1;
}

#ifdef TESTING

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
        test_item.overwrite_in_db(mongo_ctx);
    }

    if(test_item.transfer_to_user(get_caller(ctx), get_thread_id(ctx)))
        duk_push_int(ctx, test_item.get_prop_as_integer("item_id"));
    else
        push_error(ctx, "Could not transfer item to caller");

    return 1;
}
#endif // TESTING


duk_ret_t cash__expose(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    std::string from = duk_safe_get_prop_string(ctx, -1, "user");

    if(from == "")
        return push_error(ctx, "Args: user:<username>");

    auto opt_user_and_nodes = get_user_and_nodes(from, get_thread_id(ctx));

    if(!opt_user_and_nodes.has_value())
        return push_error(ctx, "No such user");

    auto hostile = opt_user_and_nodes->second.valid_hostile_actions();

    if((hostile & user_node_info::XFER_GC_FROM) > 0)
    {
        push_duk_val(ctx, opt_user_and_nodes->first.cash);
        return 1;
    }
    else
    {
        return push_error(ctx, "System Breach Node Secured");
    }

    return 1;
}


duk_ret_t item__expose(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    int pretty = !dukx_is_prop_truthy(ctx, -1, "array");
    int full = dukx_is_prop_truthy(ctx, -1, "full");

    std::string from = duk_safe_get_prop_string(ctx, -1, "user");

    if(from == "")
        return push_error(ctx, "Args: user:<username>");

    auto opt_user_and_nodes = get_user_and_nodes(from, get_thread_id(ctx));

    if(!opt_user_and_nodes.has_value())
        return push_error(ctx, "No such user");

    //user& usr = opt_user_and_nodes->first;

    //printf("%i num\n", usr.upgr_idx.size());
    //std::cout << "name " << usr.name << std::endl;

    auto hostile = opt_user_and_nodes->second.valid_hostile_actions();

    if((hostile & user_node_info::XFER_ITEM_FROM) > 0)
    {
        push_internal_items_view(ctx, pretty, full, opt_user_and_nodes->second, opt_user_and_nodes->first);

        return 1;
    }
    else
    {
        return push_error(ctx, "System Breach Node Secured");
    }

    return 1;
}


duk_ret_t handle_confirmed(duk_context* ctx, bool confirm, const std::string& username, double price)
{
    std::optional opt_user = get_user(username, get_thread_id(ctx));

    if(!opt_user.has_value())
        return push_error(ctx, "No such user");

    if(isnanf(price))
        return push_error(ctx, "NaN");

    if(!confirm)
        return push_error(ctx, "Please confirm:true to pay " + std::to_string((int)price));

    if(opt_user->cash < price)
        return push_error(ctx, "Please acquire more wealth");

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));
        opt_user->cash -= price;

        opt_user->overwrite_user_in_db(mongo_ctx);
    }

    return 0;
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

duk_ret_t item__steal(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    std::string from = duk_safe_get_prop_string(ctx, -1, "user");

    ///if you pass in an array we get nan, which seems to convert to 0
    int item_idx = duk_get_prop_string_as_int(ctx, -1, "idx", -1);

    if(from == "" || item_idx < 0)
        return push_error(ctx, "Args: user:<username>, idx:item_offset");

    auto found = get_user_and_nodes(from, get_thread_id(ctx));

    if(!found.has_value())
        return push_error(ctx, "Error or no such user");

    user& found_user = found->first;
    user_nodes& nodes = found->second;

    auto hostile = nodes.valid_hostile_actions();

    if(!((hostile & user_node_info::XFER_ITEM_FROM) > 0))
        return push_error(ctx, "System Breach Node Secured");

    ///unloads item if loaded
    #if 0
    std::string accum;
    auto ret = load_item_raw(-1, -1, item_idx, found_user, nodes, accum, get_thread_id(ctx));

    if(ret != "")
        return push_error(ctx, ret);
    #endif // 0

    std::string item_id = found_user.index_to_item(item_idx);
    int cost = 20;
    bool loaded_lock = false;

    ///make sure to move this check way below so it cant be exploited
    if(item_id == "")
        return push_error(ctx, "No such item");

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

        item it;
        it.load_from_db(mongo_ctx, item_id);

        if(it.get_prop_as_integer("item_type") == item_types::LOCK && nodes.any_contains_lock(item_id))
        {
            cost = 50;
            loaded_lock = true;
        }
    }

    bool confirm = dukx_is_prop_truthy(ctx, -1, "confirm");

    if(handle_confirmed(ctx, confirm, get_caller(ctx), cost))
        return 1;

    if(loaded_lock)
    {
        nodes.reset_all_breach();

        for(auto& i : nodes.nodes)
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

            std::vector<item> all_locks = i.get_locks(mongo_ctx);

            for(item& it : all_locks)
            {
                it.force_rotate();

                it.overwrite_in_db(mongo_ctx);
            }
        }

        {
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));
            nodes.overwrite_in_db(node_ctx);
        }
    }

    std::string accum;
    auto ret = load_item_raw(-1, -1, item_idx, found_user, nodes, accum, get_thread_id(ctx));

    if(ret != "")
        return push_error(ctx, ret);

    push_xfer_item_with_logs(ctx, item_idx, from, get_caller(ctx));
    return 1;
}


duk_ret_t cash__steal(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string from = duk_safe_get_prop_string(ctx, -1, "user");

    if(from == "")
        return push_error(ctx, "Args: user:<username>, amount:<number>");

    double amount = duk_safe_get_generic_with_guard(duk_get_number, duk_is_number, ctx, -1, "amount", 0.0);

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



duk_ret_t nodes__view_log(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = duk_safe_get_prop_string(ctx, -1, "user");

    int make_array = dukx_is_prop_truthy(ctx, -1, "array");

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

duk_ret_t hack_internal(priv_context& priv_ctx, duk_context* ctx, const std::string& name_of_person_being_attacked)
{
    //std::cout << "user_name " << name_of_person_being_attacked << std::endl;

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

        nodes.leave_trace(*current_node, attacker.name, usr, get_thread_id(ctx));

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

                ///todo: send a chats.tell to victim here
            }

            if(i.is_breached())
                continue;

            std::string func = i.get_prop("lock_type");

            auto it = secret_map.find(func);

            ///is a lock
            if(it != secret_map.end())
            {
                if(!it->second.ptr(priv_ctx, ctx, msg, i, name_of_person_being_attacked))
                {
                    all_success = false;

                    break;
                }
                else
                {
                    ///todo: send a chats.tell to victim here
                    i.breach();

                    create_notification(get_thread_id(ctx), name_of_person_being_attacked, make_notif_col("-" + i.get_prop("short_name") + " breached-"));

                    mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));
                    i.overwrite_in_db(item_ctx);
                }
            }
        }
    }

    user_node* breach_node = nodes.type_to_node(user_node_info::BREACH);

    if(breach_node == nullptr)
        push_error(ctx, "Error Code: Yellow Panther in hack_internal (net.hack?)");

    bool breach_is_breached = breach_node->is_breached();

    if(current_node->is_breached())
    {
        msg += current_node->get_breach_message(nodes);
    }

    ///do info here first, then display the breach message the next time round
    finalise_info(msg, all_success, current_node->is_breached(), attackables.size());

    if(all_success && !current_node->is_breached())
    {
        msg += current_node->get_breach_message(nodes);

        create_notification(get_thread_id(ctx), name_of_person_being_attacked, make_error_col("-" + user_node_info::long_names[current_node->type] + " Node Compromised-"));
    }

    if(all_success)
    {
        current_node->breach();

        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        nodes.overwrite_in_db(node_ctx);
    }

    if(breach_node->is_breached() && !breach_is_breached)
    {
        std::vector<item> all_items;

        {
            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

            all_items = usr.get_all_items(item_ctx);
        }

        for(item& it : all_items)
        {
            if(it.get_prop_as_integer("item_type") == item_types::ON_BREACH && usr.has_loaded_item(it.get_prop("item_id")))
            {
                std::string script_name = it.get_prop("script_name");

                script_name = on_breach_name_to_real_script_name(script_name, usr.name);

                if(script_name == "")
                    continue;

                script_name = "#" + script_name + "({attacker:\"" + get_caller(ctx) + "\"})";

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

    duk_push_string(ctx, msg.c_str());

    return 1;
}

#ifdef USE_LOCS

duk_ret_t user__port(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = get_host_from_fullname(priv_ctx.called_as);

    return hack_internal(priv_ctx, ctx, name_of_person_being_attacked);
}
#endif // USE_LOCS


duk_ret_t net__hack(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string name_of_person_being_attacked = duk_safe_get_prop_string(ctx, -1, "user");

    if(name_of_person_being_attacked == "")
        return push_error(ctx, "Usage: net.hack({user:<name>})");

    if(!get_user(name_of_person_being_attacked, get_thread_id(ctx)))
        return push_error(ctx, "No such user");

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_npc_properties_context(get_thread_id(ctx));

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

        auto path = playspace_network_manage.get_accessible_path_to(ctx, name_of_person_being_attacked, get_caller(ctx), (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS), -1, hack_cost);

        if(path.size() == 0)
            return push_error(ctx, "No Path");

        playspace_network_manage.modify_path_per_link_strength_with_logs(path, -hack_cost, {"Hostile Path Access"}, get_thread_id(ctx));
    }

    return hack_internal(priv_ctx, ctx, name_of_person_being_attacked);
}


duk_ret_t nodes__manage(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    bool get_array = dukx_is_prop_truthy(ctx, -1, "array");

    std::string usage = "Usage: " + make_key_val("swap", "[idx1, idx2]");

    ///reorder
    bool has_arr = dukx_is_prop_truthy(ctx, -1, "swap");

    user usr;

    {
        mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!usr.load_from_db(user_ctx, get_caller(ctx)))
            return push_error(ctx, "No such user, really bad error");
    }

    user_nodes nodes;

    {
        mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

        ///yeah this isn't good enough, need to do what we did for locs?
        ///or just do it in loc handler i guess
        nodes.ensure_exists(node_ctx, get_caller(ctx));

        nodes.load_from_db(node_ctx, get_caller(ctx));
    }

    if(has_arr)
    {
        if(sl > 1)
            return push_error(ctx, "Must be called with a sec level of 1 to swap");

        duk_get_prop_string(ctx, -1, "swap");
        int len = duk_get_length(ctx, -1);

        if(len != 2)
            return push_error(ctx, "array len != 2");

        std::vector<int> values;

        ///iterate array, being extremely generous with what you can pass in
        for(int i = 0; i < len; i++)
        {
            duk_get_prop_index(ctx, -1, i);

            if(duk_is_number(ctx, -1))
                values.push_back(duk_get_int(ctx, -1));

            duk_pop(ctx);
        }

        duk_pop(ctx);

        if(values.size() != 2)
            return push_error(ctx, "array len != 2");

        std::vector<std::string> items;

        for(auto& i : values)
        {
            std::string item = usr.index_to_item(i);

            if(item == "")
                return push_error(ctx, "Item does not exist");

            items.push_back(item);
        }

        auto u1 = nodes.lock_to_node(items[0]);
        auto u2 = nodes.lock_to_node(items[1]);

        if(!u1.has_value() || !u2.has_value())
            return push_error(ctx, "Item not found on node");

        auto p1 = u1.value()->lock_to_pointer(items[0]);
        auto p2 = u2.value()->lock_to_pointer(items[1]);

        if(!p1.has_value() || !p2.has_value())
            return push_error(ctx, "Item not found on node");

        ///can cause multiples to be loaded on one stack
        std::swap(*p1.value(), *p2.value());

        /*{
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));

            u1.value()->breach();
            u2.value()->breach();

            nodes.overwrite_in_db(node_ctx);
        }*/

        {
            mongo_lock_proxy node_ctx = get_global_mongo_node_properties_context(get_thread_id(ctx));
            nodes.overwrite_in_db(node_ctx);
        }

        {
            mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

            item i1, i2;
            i1.load_from_db(items_ctx, items[0]);
            i2.load_from_db(items_ctx, items[1]);

            i1.breach();
            i2.breach();

            i1.overwrite_in_db(items_ctx);
            i2.overwrite_in_db(items_ctx);
        }

        return push_success(ctx);
    }


    if(!get_array)
    {
        std::string accum = usage + "\n" + "Node Key: ";

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
    }
    else
    {
        mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(get_thread_id(ctx));

        push_duk_val(ctx, nodes.get_as_json(item_ctx, usr));
    }

    return 1;
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
duk_ret_t net__map(priv_context& priv_ctx, duk_context* ctx, int sl)
{
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
}

std::vector<nlohmann::json> get_net_view_data_arr(network_accessibility_info& info)
{
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    std::vector<json> all_npc_data;

    for(auto& i : info.ring_ordered_names)
    {
        const std::string& name = i;
        vec3f pos = info.global_pos[name];

        json j;
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

    for(json& j : all_npc_data)
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

duk_ret_t net__view(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string from = duk_safe_get_prop_string(ctx, -1, "user");
    int num = duk_get_prop_string_as_int(ctx, -1, "n", 2);

    bool arr = dukx_is_prop_truthy(ctx, -1, "array");

    if(from == "")
        return push_error(ctx, "usage: net.view({user:<username>, n:6})");

    if(num < 0 || num > 15)
        return push_error(ctx, "n out of range [1,15]");

    auto opt_user_and_nodes = get_user_and_nodes(from, get_thread_id(ctx));

    if(!opt_user_and_nodes.has_value())
        return push_error(ctx, "User does not exist");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(!playspace_network_manage.has_accessible_path_to(ctx, from, get_caller(ctx), (path_info::path_info)(path_info::VIEW_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
        return push_error(ctx, "Target Inaccessible");

    user& usr = opt_user_and_nodes->first;

    auto hostile_actions = opt_user_and_nodes->second.valid_hostile_actions();

    if(!usr.is_allowed_user(get_caller(ctx)) && usr.name != get_caller(ctx) && !((hostile_actions & user_node_info::VIEW_LINKS) > 0))
        return push_error(ctx, "Node is Locked");

    network_accessibility_info info = playspace_network_manage.generate_network_accessibility_from(ctx, from, num);


    ///so
    ///the information we want to give back to the client wants to be very rich
    ///we need the connections of every npc if we have permission
    ///path to original player? unsure on this
    ///position

    using nlohmann::json;

    std::vector<json> all_npc_data = get_net_view_data_arr(info);

    json final_data = all_npc_data;

    if(arr)
        push_duk_val(ctx, final_data);
    else
    {
        std::string str = get_net_view_data_str(all_npc_data);

        push_duk_val(ctx, str);
    }

    return 1;
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

    std::vector<std::string> path = playspace_network_manage.get_accessible_path_to(ctx, target, host, path_info::USE_LINKS, -1, link_stability_cost);

    if(path.size() == 0)
    {
        auto pseudo_path = playspace_network_manage.get_accessible_path_to(ctx, target, host, path_info::USE_LINKS, -1);

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

    if(!playspace_network_manage.has_accessible_path_to(ctx, start, get_caller(ctx), path_info::VIEW_LINKS))
        return push_error(ctx, "No path to start user");

    std::vector<std::string> viewable_distance;

    if(path_type == "view")
        viewable_distance = playspace_network_manage.get_accessible_path_to(ctx, target, start, path_info::VIEW_LINKS, -1, minimum_stability);
    else
        viewable_distance = playspace_network_manage.get_accessible_path_to(ctx, target, start, path_info::USE_LINKS, -1, minimum_stability);

    ///STRANGER DANGER
    std::vector<std::string> link_path = playspace_network_manage.get_accessible_path_to(ctx, target, start, (path_info::path_info)(path_info::NONE | path_info::ALLOW_WARP_BOUNDARY), -1, minimum_stability);

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

duk_ret_t create_and_modify_link(duk_context* ctx, const std::string& from, const std::string& user_1, const std::string& target, bool create, double stab, bool confirm, bool enforce_connectivity, std::string path_type = "use")
{
    std::optional opt_user_and_nodes_1 = get_user_and_nodes(user_1, get_thread_id(ctx));
    std::optional opt_user_and_nodes_2 = get_user_and_nodes(target, get_thread_id(ctx));

    if(!opt_user_and_nodes_1.has_value())
        return push_error(ctx, "No such user (user)");

    if(!opt_user_and_nodes_2.has_value())
        return push_error(ctx, "No such user (target)");

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(!create && !playspace_network_manage.has_accessible_path_to(ctx, user_1, from, (path_info::path_info)(path_info::VIEW_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
        return push_error(ctx, "No currently visible path to user");

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
        if(!playspace_network_manage.has_accessible_path_to(ctx, target, user_1, (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
            return push_error(ctx, "No path from user to target");
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
            if(handle_confirmed(ctx, confirm, get_caller(ctx), price))
                return 1;

            std::vector<std::string> path;

            //if(path_type == "direct")
            //    path = playspace_network_manage.get_accessible_path_to(ctx, target, usr, path_info::NONE);
            if(path_type == "view")
                path = playspace_network_manage.get_accessible_path_to(ctx, target, user_1, path_info::VIEW_LINKS);
            if(path_type == "use")
                path = playspace_network_manage.get_accessible_path_to(ctx, target, user_1, path_info::USE_LINKS);

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

            if(playspace_network_manage.current_network_links(user_1) >= playspace_network_manage.max_network_links(user_1) ||
               playspace_network_manage.current_network_links(target) >= playspace_network_manage.max_network_links(target))
                return push_error(ctx, "No spare links");

            scheduled_tasks& task_sched = get_global_scheduled_tasks();

            task_sched.task_register(task_type::ON_HEAL_NETWORK, 10.f, {user_1, target, std::to_string(stab)}, get_thread_id(ctx));

            return push_success(ctx, "Link creation scheduled in 10s");
        }

        push_duk_val(ctx, "Schedule new connection?");
        return 1;
    }

    return 0;
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
            names.push_back("Name: " + (std::string)i["name"]);

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

    bool centre = dukx_is_prop_truthy(ctx, -1, "centre");

    user my_user;

    {
        mongo_lock_proxy lock = get_global_mongo_user_info_context(-2);

        if(!my_user.load_from_db(lock, get_caller(ctx)))
            return push_error(ctx, "Error: Does not exist");
    }

    int n_val = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", -1);
    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");
    //int found_width = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", -1);

    int found_w = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "w", 160);
    int found_h = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "h", 80);

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
            info.rings[*i.name] = 0;
            info.global_pos[*i.name] = i.get_pos();
            info.ring_ordered_names.push_back(*i.name);
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

        std::string result = ascii_render_from_accessibility_info(info, buffer, pos, 0.07f, flags, *structure.name);

        result = "Current Sys: " + colour_string(*structure.name) + "\n" + result;

        push_duk_val(ctx, result);
    }
    else
    {
        nlohmann::json all_data;

        std::vector<nlohmann::json> data;

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

            data.push_back(j);
        }

        all_data = data;

        push_duk_val(ctx, all_data);
    }

    return 1;
}

duk_ret_t sys__view(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    std::string str = duk_safe_get_prop_string(ctx, -1, "sys");
    bool is_arr = dukx_is_prop_truthy(ctx, -1, "array");
    std::string found_target = duk_safe_get_prop_string(ctx, -1, "user");

    int found_w = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "w", 80);
    int found_h = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "h", 40);

    found_w = clamp(found_w, 5, 300);
    found_h = clamp(found_h, 5, 200);

    bool has_target = found_target.size() > 0;

    if(found_target == "")
        found_target = get_caller(ctx);

    user target_user;

    {
        mongo_lock_proxy lock = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!target_user.load_from_db(lock, found_target))
            return push_error(ctx, "Error: Target does not exist");
    }

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    user my_user;

    {
        mongo_lock_proxy lock = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!my_user.load_from_db(lock, get_caller(ctx)))
            return push_error(ctx, "Error: Does not exist");
    }

    int n_count = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", -1);

    if(n_count < -1)
        n_count = -1;

    if(n_count > 99)
        n_count = 99;

    std::optional<low_level_structure*> opt_structure;

    if(str == "")
        opt_structure = low_level_structure_manage.get_system_of(my_user);
    else
        opt_structure = low_level_structure_manage.get_system_from_name(str);

    if(!opt_structure.has_value())
        return push_error(ctx, "You are lost, there is no help for you now");

    /*std::cout <<" tlkinks " << playspace_network_manage.current_network_links(my_user.name) << std::endl;

    auto my_links = playspace_network_manage.get_links(my_user.name);

    for(auto& i : my_links)
    {
        std::cout << "fl " << i << std::endl;
    }*/

    low_level_structure& structure = *opt_structure.value();

    std::vector<user> special_users = structure.get_special_users(get_thread_id(ctx));
    std::vector<user> all_users;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));
        all_users = structure.get_all_users(mongo_ctx);
    }

    std::vector<std::vector<std::string>> buffer = ascii_make_buffer({found_w, found_h}, false);

    network_accessibility_info info;

    bool visited_host_user = false;

    if(!has_target)
    {
        for(user& usr : special_users)
        {
            network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(ctx, usr.name, n_count);

            std::vector<std::string> connected = structure.get_connected_systems_from(usr.name);

            if(connected.size() > 0)
            {
                cur.extra_data_map[usr.name] += "(";
            }

            for(int i=0; i < (int)connected.size(); i++)
            {
                if(i != (int)connected.size() - 1)
                    cur.extra_data_map[usr.name] += colour_string(connected[i]) + ", ";
                else
                    cur.extra_data_map[usr.name] += colour_string(connected[i]);
            }

            if(connected.size() > 0)
            {
                cur.extra_data_map[usr.name] += ")";
            }

            info = network_accessibility_info::merge_together(info, cur);

            if(usr.name == target_user.name)
                visited_host_user = true;
        }

        ///investigate this for being incredibly terrible
        for(user& usr : all_users)
        {
            if(playspace_network_manage.current_network_links(usr.name) > 0)
                continue;

            network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(ctx, usr.name, n_count);

            auto old_names = cur.ring_ordered_names;

            if(playspace_network_manage.current_network_links(usr.name) == 0)
            {
                cur.extra_data_map[usr.name] = "(free)";
            }

            info = network_accessibility_info::merge_together(info, cur);

            if(usr.name == target_user.name)
                visited_host_user = true;
        }
    }

    if(!visited_host_user)
    {
        network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(ctx, target_user.name, n_count);

        info = network_accessibility_info::merge_together(info, cur);
    }

    if(!is_arr)
    {
        std::string from = get_caller(ctx);

        vec3f pos = {0,0,0};
        ///info.global_pos[from]

        float current_scale = 0.5f;

        if(has_target)
            current_scale = 1.f;

        std::string result = ascii_render_from_accessibility_info(info, buffer, pos, current_scale, ascii::NONE);

        result = "Current Sys: " + colour_string(*structure.name) + "\n" + result;

        push_duk_val(ctx, result);
    }
    else
    {
        std::vector<json> all_npc_data;

        for(auto& i : info.ring_ordered_names)
        {
            const std::string& name = i;
            vec3f pos = info.global_pos[name];

            json j;
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

        json final_data = all_npc_data;

        push_duk_val(ctx, final_data);
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

    return 1;
}

duk_ret_t sys__move(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    bool has_to = dukx_is_prop_truthy(ctx, -1, "to");
    bool has_confirm = dukx_is_prop_truthy(ctx, -1, "confirm");
    bool has_stop = dukx_is_prop_truthy(ctx, -1, "stop");
    bool has_queue = dukx_is_prop_truthy(ctx, -1, "queue");
    std::optional<user> my_user_opt = get_user(get_caller(ctx), get_thread_id(ctx));

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

        std::string msg = "Please specify " + make_key_val("to", "\"user\"") + ", " +
                                              make_key_val("to", "\"system\"") + ", or " +
                                              make_key_val("to", "[x, y, z]");

        push_duk_val(ctx, str + "\n" + msg);
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

            vec3f current_pos;

            if(!has_queue)
                current_pos = my_user.get_local_pos();
            else
                current_pos = my_user.get_final_pos().position;

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
duk_ret_t sys__access(priv_context& priv_ctx, duk_context* ctx, int sl)
{
    COOPERATE_KILL();

    if(!dukx_is_prop_truthy(ctx, -1, "user"))
        return push_error(ctx, "Takes a user parameter");

    std::string target_name = duk_safe_get_prop_string(ctx, -1, "user");
    bool has_activate = dukx_is_prop_truthy(ctx, -1, "activate");
    bool has_queue = dukx_is_prop_truthy(ctx, -1, "queue");
    bool has_connect = dukx_is_prop_truthy(ctx, -1, "connect");
    bool has_disconnect = dukx_is_prop_truthy(ctx, -1, "disconnect");
    bool has_modify = duk_has_prop_string(ctx, -1, "modify");
    bool has_confirm = dukx_is_prop_truthy(ctx, -1, "confirm");
    bool has_arr = dukx_is_prop_truthy(ctx, -1, "array");
    bool has_users = dukx_is_prop_truthy(ctx, -1, "users");

    std::string add_user = duk_safe_get_prop_string(ctx, -1, "add");
    std::string remove_user = duk_safe_get_prop_string(ctx, -1, "remove");
    bool view_users = dukx_is_prop_truthy(ctx, -1, "view");

    /*int n_count = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "n", 1);
    n_count = clamp(n_count, 1, 100);*/

    int n_count = 1;

    user target;
    user my_user;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

        if(!target.load_from_db(mongo_ctx, target_name))
            return push_error(ctx, "Invalid user");

        if(!my_user.load_from_db(mongo_ctx, get_caller(ctx)))
            return push_error(ctx, "Invalid host, really bad");
    }

    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    auto target_sys_opt = low_level_structure_manage.get_system_of(target.name);
    auto my_sys_opt = low_level_structure_manage.get_system_of(my_user.name);

    if(!target_sys_opt.has_value() || !my_sys_opt.has_value())
        return push_error(ctx, "Well then you are lost (high ground!)");

    if(target_sys_opt.value() != my_sys_opt.value())
        return push_error(ctx, "Not in the same system");

    if(!playspace_network_manage.has_accessible_path_to(ctx, target.name, my_user.name, (path_info::path_info)(path_info::USE_LINKS | path_info::TEST_ACTION_THROUGH_WARP_NPCS)))
        return push_error(ctx, "No path");

    low_level_structure& current_sys = *my_sys_opt.value();

    bool is_warpy = false;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_npc_properties_context(get_thread_id(ctx));

        is_warpy = npc_info::has_type(mongo_ctx, npc_info::WARPY, target.name);
    }

    user_nodes target_nodes = get_nodes(target.name, get_thread_id(ctx));

    auto valid_actions = target_nodes.valid_hostile_actions();
    bool can_modify_users = (target.is_allowed_user(get_caller(ctx)) || ((valid_actions & user_node_info::CLAIM_NPC) > 0)) && target.is_npc() && !is_warpy;

    std::string total_msg;

    space_pos_t my_local_pos = my_user.get_local_pos();

    std::string situation_string = "Location: [" +
                                        to_string_with_enforced_variable_dp(my_local_pos.x(), 2) + ", " +
                                        to_string_with_enforced_variable_dp(my_local_pos.y(), 2) + ", " +
                                        to_string_with_enforced_variable_dp(my_local_pos.z(), 2) + "]\n";

    total_msg += situation_string;

    //std::string sector_string = "Sector: " + usr.fetch_sector();
    //total_msg += sector_string;

    std::string system_string = "System: " + colour_string(*current_sys.name);
    total_msg += system_string + "\n";

    double maximum_warp_distance = MAXIMUM_WARP_DISTANCE;

    float distance = (target.get_local_pos() - my_user.get_local_pos()).length();

    nlohmann::json array_data;

    array_data["distance"] = distance;

    array_data["is_long_distance_traveller"] = is_warpy;

    if(is_warpy)
    {
        std::vector<std::string> connected = playspace_network_manage.get_links(target.name);
        std::vector<user> connected_users = load_users(connected, get_thread_id(ctx));

        std::string connected_system;
        low_level_structure* found_system = nullptr;
        user* destination_user = nullptr;

        for(user& usr : connected_users)
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_npc_properties_context(get_thread_id(ctx));

            if(npc_info::has_type(mongo_ctx, npc_info::WARPY, usr.name))
            {
                auto connected_sys_opt = low_level_structure_manage.get_system_of(usr);

                if(!connected_sys_opt.has_value() || connected_sys_opt.value() == target_sys_opt.value())
                    continue;

                low_level_structure& structure = *connected_sys_opt.value();

                connected_system = *structure.name;
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

                    create_notification(get_thread_id(ctx), my_user.name, make_notif_col("-Arrived at " + connected_system + "-"));
                }
            }
        }

        ///should also print sys.view map
        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

            my_user.overwrite_user_in_db(mongo_ctx);
        }
    }

    std::string links_string = "";

    {
        network_accessibility_info info = playspace_network_manage.generate_network_accessibility_from(ctx, target.name, n_count);

        std::vector<json> all_npc_data = get_net_view_data_arr(info);

        array_data["links"] = all_npc_data;

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

        if(!has_users && get_caller(ctx) != target.name)
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
                            mongo_lock_proxy mongo_ctx = get_global_mongo_user_accessible_context(get_thread_id(ctx));

                            my_user.overwrite_user_in_db(mongo_ctx);
                        }

                        duk_ret_t found = create_and_modify_link(ctx, my_user.name, my_user.name, target.name, true, 10.f, has_confirm, true);

                        if(found == 1)
                            return 1;
                        else
                            return push_error(ctx, "Could not Link");
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

    if(!has_users)
    {
        ///must have a path for us to be able to access the npc
        if(!has_modify)
        {
            total_msg += "Pass " + make_key_val("modify", "num") + " to strengthen or weaken the path to this target\n";

            array_data["can_modify_links"] = true;
        }
        else
        {
            double amount = duk_safe_get_generic_with_guard(duk_get_int, duk_is_number, ctx, -1, "modify", 0);

            if(amount == 0)
            {
                total_msg += "Modify should be greater or less than 0\n";
            }
            else
            {
                duk_ret_t found = create_and_modify_link(ctx, my_user.name, my_user.name, target.name, false, amount, has_confirm, true);

                if(found == 1)
                    return 1;
                else
                    return push_error(ctx, "Could not modify link");
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


            int add_price = 200;
            int remove_price = 100;
            int view_price = 20;

            if(target.is_allowed_user(get_caller(ctx)))
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
                if(!target.is_allowed_user(get_caller(ctx)) && handle_confirmed(ctx, has_confirm, get_caller(ctx), view_price))
                    return 1;

                std::string ret;

                if(target.is_allowed_user(get_caller(ctx)))
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
                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

                    user_is_valid = user().load_from_db(mongo_ctx, add_user);
                }

                if(!target.is_npc())
                    return push_error(ctx, "Cannot take over a user");

                if(user_is_valid)
                {
                    if(!target.is_allowed_user(get_caller(ctx)) && handle_confirmed(ctx, has_confirm, get_caller(ctx), add_price))
                        return 1;

                    total_msg += make_success_col("Added User") + ": " + colour_string(get_caller(ctx)) + "\n";

                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

                    target.add_allowed_user(add_user, mongo_ctx);
                    target.overwrite_user_in_db(mongo_ctx);

                    array_data["added_user"] = add_user;
                }
                else
                {
                    return push_error(ctx, "Add User is not valid (" + add_user + ")");
                }
            }

            if(remove_user != "")
            {
                bool user_is_valid = false;

                {
                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

                    user_is_valid = user().load_from_db(mongo_ctx, remove_user);
                }

                if(user_is_valid)
                {
                    if(!target.is_allowed_user(get_caller(ctx)) && handle_confirmed(ctx, has_confirm, get_caller(ctx), remove_price))
                        return 1;

                    total_msg += make_success_col("Removed User") + ": " + colour_string(get_caller(ctx)) + "\n";

                    mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(get_thread_id(ctx));

                    target.remove_allowed_user(remove_user, mongo_ctx);
                    target.overwrite_user_in_db(mongo_ctx);

                    array_data["removed_user"] = remove_user;
                }
                else
                {
                    return push_error(ctx, "Remove User is not valid (" + remove_user + ")");
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
        push_duk_val(ctx, array_data);
        return 1;
    }

    //total_msg += links_string;

    if(total_msg == "")
        return 0;

    if(total_msg.size() > 0 && total_msg.back() == '\n')
        total_msg.pop_back();

    push_duk_val(ctx, total_msg);
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

    network_accessibility_info info;

    for(user& usr : special_users)
    {
        network_accessibility_info cur = playspace_network_manage.generate_network_accessibility_from(ctx, usr.name, 15);

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
