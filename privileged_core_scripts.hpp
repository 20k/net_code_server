#ifndef PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
#define PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED

#include <secret/tutorial.hpp>
#include "unified_scripts.hpp"
#include <string>

struct user;
struct item;
struct user_nodes;

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
using function_priv_new_t = js::value (*)(priv_context&, js::value_context& vctx, js::value& arg, int);

inline
bool can_run(int csec_level, int maximum_sec)
{
    return csec_level <= maximum_sec;
}

///could potentially use __FUNCTION__ here
///as it should work across msvc/gcc/clang... but... technically not portable
#define SL_GUARD(x) if(!can_run(sl, x)){ return js::make_error(*vctx, "Security level guarantee failed"); }

struct priv_func_info
{
    function_priv_t func_duk = nullptr;
    function_priv_new_t func_new = nullptr;
    int sec_level = 0;
    bool is_privileged = false; ///can only be called by something privileged

    priv_func_info(function_priv_t _func, int _sec_level, bool _is_privileged = false)
    {
        func_duk = _func;
        sec_level = _sec_level;
        is_privileged = _is_privileged;
    }

    priv_func_info(function_priv_new_t _func, int _sec_level, bool _is_privileged = false)
    {
        func_new = _func;
        sec_level = _sec_level;
        is_privileged = _is_privileged;
    }
};

extern
std::map<std::string, priv_func_info> privileged_functions;

struct script_arg
{
    std::string key;
    std::string val;
};

///so say this is midsec
///we can run if the sl is midsec or lower
///lower sls are less secure

///hmm. Maybe we want to keep sls somewhere which is dynamically editable like global properties in the db
///cache the calls, and like, refresh the cache every 100 calls or something

js::value cash__balance(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value scripts__get_level(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

std::string format_pretty_names(const std::vector<std::string>& names);

js::value scripts__me(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value scripts__public(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value scripts__info(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value cash_internal_xfer(js::value_context& vctx, const std::string& from, const std::string& to, double amount, bool pvp_action);
///TODO: TRANSACTION HISTORY

js::value cash__xfer_to(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value cash__xfer_to_caller(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

///this is only valid currently, will need to expand to hardcode in certain folders

js::value scripts__core(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

size_t get_wall_time();
double get_wall_time_s();

bool user_in_channel(mongo_lock_proxy& mongo_ctx, const std::string& username, const std::string& channel);
std::vector<std::string> get_users_in_channel(mongo_lock_proxy& mongo_ctx, const std::string& channel);

bool is_valid_channel_name(const std::string& in);

js::value channel__create(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value channel__join(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value channel__list(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value channel__leave(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value msg__manage(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value msg__send(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value msg__tell(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

void create_notification(int lock_id, const std::string& to, const std::string& notif_msg);
void create_xfer_notif(js::value_context& vctx, const std::string& xfer_from, const std::string& xfer_to, double amount);
void create_xfer_item_notif(js::value_context& vctx, const std::string& xfer_from, const std::string& xfer_to, const std::string& item_name);
void create_destroy_item_notif(js::value_context& vctx, const std::string& to, const std::string& item_name);

///formats time
std::string format_time(const std::string& in);
std::string prettify_chat_strings(std::vector<nlohmann::json>& found, bool use_channels = true);

js::value msg__recent(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value users__me(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value users__accessible(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

#if 0
///pretty tired when i wrote this check it for mistakes

duk_ret_t sys__disown_upg(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t sys__xfer_upgrade_uid(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // 0

std::string escape_str(std::string in);

std::string format_item(item& i, bool is_short, user& usr, user_nodes& nodes);

//duk_object_t get_item_raw(item& i, bool is_short, user& usr, user_nodes& nodes);

/*
void change_item_raw(mongo_lock_proxy& mongo_ctx, int load_idx, int unload_idx, user& found_user);*/


std::string load_item_raw(int node_idx, int load_idx, int unload_idx, user& usr, user_nodes& nodes, std::string& accum, int thread_id);

js::value push_internal_items_view(js::value_context& vctx, int pretty, int full, user_nodes& nodes, user& found_user, std::string preamble, bool pvp);

js::value item__cull(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value item__manage(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value item__list(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value item__load(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value item__unload(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value push_xfer_item_with_logs(js::value_context& vctx, int item_idx, user& from, user& to, bool is_pvp);

js::value item__xfer_to(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value item__bundle_script(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value item__register_bundle(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value item__configure_on_breach(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

#if defined(TESTING) || defined(EXTRAS)
duk_ret_t item__create(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // TESTING


js::value cash__expose(priv_context& priv_ctx, js::value_context& vctx, js::value& val, int sl);
js::value item__expose(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

///handles confirm:true for cash
std::optional<js::value> handle_confirmed(js::value_context& vctx, bool confirm, const std::string& username, double price);

///have item__steal reset internal node structure

js::value item__steal(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value cash__steal(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

js::value nodes__view_log(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value log__expose(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value hack_internal(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, const std::string& name_of_person_being_attacked, bool is_arr);

#ifdef USE_LOCS
duk_ret_t user__port(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // USE_LOCS


js::value net__hack(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
#if 0
duk_ret_t net__hack_new(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // 0
js::value nodes__manage(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);

#ifdef USE_LOCS

duk_ret_t nodes__port(priv_context& priv_ctx, duk_context* ctx, int sl);
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

js::value net__view(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
js::value net__map(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
//duk_ret_t net__links(priv_context& priv_ctx, duk_context* ctx, int sl);
js::value net__switch(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
duk_ret_t net__path(priv_context& priv_ctx, duk_context* ctx, int sl);

#ifdef OLD_DEPRECATED
duk_ret_t net__modify(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__move(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__access(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t gal__map(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t gal__list(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // OLD_DEPRECATED

duk_ret_t sys__map(priv_context& priv_ctx, duk_context* ctx, int sl);
#ifdef SYSTEM_TESTING
duk_ret_t sys__debug_view(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // SYSTEM_TESTING
js::value sys__view(priv_context& priv_ctx, js::value_context& vctx, js::value& arg, int sl);
duk_ret_t sys__move(priv_context& priv_ctx, duk_context* ctx, int sl);
js::value sys__access(priv_context& priv_ctx, js::value_context& vctx, js::value& val, int sl);
duk_ret_t sys__limits(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t mission__list(priv_context& priv_ctx, duk_context* ctx, int sl);

#ifdef TESTING
duk_ret_t sys__debug(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t mission__debug(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // TESTING

#ifdef TESTING

duk_ret_t cheats__arm(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t cheats__give(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t cheats__salvage(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t cheats__task(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t cheats__disconnect(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t cheats__unlink(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t cheats__testloot(priv_context& priv_ctx, duk_context* ctx, int sl);

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
    REGISTER_FUNCTION_PRIV(cash__expose, 3),
    REGISTER_FUNCTION_PRIV(cash__xfer_to, 2),
    REGISTER_FUNCTION_PRIV(cash__xfer_to_caller, 4),
    REGISTER_FUNCTION_PRIV(cash__steal, 2),
    REGISTER_FUNCTION_PRIV(scripts__get_level, 4),
    REGISTER_FUNCTION_PRIV(scripts__core, 4),
    REGISTER_FUNCTION_PRIV(scripts__me, 2),
    REGISTER_FUNCTION_PRIV(scripts__public, 4),
    REGISTER_FUNCTION_PRIV(scripts__info, 4),
    REGISTER_FUNCTION_PRIV(channel__create, 3),
    REGISTER_FUNCTION_PRIV(channel__join, 3),
    REGISTER_FUNCTION_PRIV(channel__leave, 3),
    REGISTER_FUNCTION_PRIV(channel__list, 3),
    REGISTER_FUNCTION_PRIV(msg__manage, 3),
    REGISTER_FUNCTION_PRIV(msg__send, 3),
    REGISTER_FUNCTION_PRIV(msg__tell, 3),
    REGISTER_FUNCTION_PRIV(msg__recent, 2),
    REGISTER_FUNCTION_PRIV(users__me, 0),
    REGISTER_FUNCTION_PRIV(users__accessible, 0),
    #if defined(TESTING) || defined(EXTRAS)
    REGISTER_FUNCTION_PRIV(item__create, 0),
    #endif // TESTING
    REGISTER_FUNCTION_PRIV(item__steal, 1),
    REGISTER_FUNCTION_PRIV(item__expose, 3),
    //REGISTER_FUNCTION_PRIV(sys__disown_upg, 0),
    //REGISTER_FUNCTION_PRIV(sys__xfer_upgrade_uid, 0),
    REGISTER_FUNCTION_PRIV(item__xfer_to, 1),
    REGISTER_FUNCTION_PRIV(item__manage, 2),
    REGISTER_FUNCTION_PRIV(item__list, 2),
    REGISTER_FUNCTION_PRIV(item__load, 2),
    REGISTER_FUNCTION_PRIV(item__unload, 2),
    REGISTER_FUNCTION_PRIV(item__cull, 1),
    REGISTER_FUNCTION_PRIV(item__bundle_script, 1),
    REGISTER_FUNCTION_PRIV(item__register_bundle, 0),
    REGISTER_FUNCTION_PRIV(item__configure_on_breach, 1),
    //REGISTER_FUNCTION_PRIV(user__port, 0), ///should this exist? It has to currently for dumb reasons ///nope, it needs special setup
    REGISTER_FUNCTION_PRIV(nodes__manage, 1),
    #ifdef USE_LOCS
    REGISTER_FUNCTION_PRIV(nodes__port, 1),
    #endif // USE_LOCS
    REGISTER_FUNCTION_PRIV(nodes__view_log, 1),
    REGISTER_FUNCTION_PRIV(log__expose, 1),
    REGISTER_FUNCTION_PRIV(net__view, 1),
    REGISTER_FUNCTION_PRIV(net__map, 1),
    REGISTER_FUNCTION_PRIV(net__hack, 0),
    #ifdef OLD_DEPRECATED
    REGISTER_FUNCTION_PRIV(net__access, 0),
    REGISTER_FUNCTION_PRIV(net__move, 0),
    #endif // OLD_DEPRECATED
    REGISTER_FUNCTION_PRIV(net__switch, 0),
    REGISTER_FUNCTION_PRIV(net__path, 0),
    #ifdef OLD_DEPRECATED
    REGISTER_FUNCTION_PRIV(net__modify, 0),
    REGISTER_FUNCTION_PRIV(gal__map, 1),
    REGISTER_FUNCTION_PRIV(gal__list, 4),
    #endif // OLD_DEPRECATED
    #ifdef TESTING
    REGISTER_FUNCTION_PRIV(cheats__arm, 4),
    REGISTER_FUNCTION_PRIV(cheats__salvage, 4),
    REGISTER_FUNCTION_PRIV(cheats__give, 4),
    REGISTER_FUNCTION_PRIV(cheats__task, 4),
    REGISTER_FUNCTION_PRIV(cheats__disconnect, 4),
    REGISTER_FUNCTION_PRIV(cheats__unlink, 4),
    REGISTER_FUNCTION_PRIV(cheats__testloot, 4),
    //REGISTER_FUNCTION_PRIV(net__hack_new, 0),
    #endif // TESTING
    REGISTER_FUNCTION_PRIV(sys__map, 1),
    #ifdef SYSTEM_TESTING
    REGISTER_FUNCTION_PRIV(sys__debug_view, 1),
    #endif // SYSTEM_TESTING
    REGISTER_FUNCTION_PRIV(sys__view, 1),
    REGISTER_FUNCTION_PRIV(sys__move, 0),
    REGISTER_FUNCTION_PRIV(sys__access, 0),
    REGISTER_FUNCTION_PRIV(sys__limits, 2),
    REGISTER_FUNCTION_PRIV(ada__access, 4),
    REGISTER_FUNCTION_PRIV(able__help, 4),
    REGISTER_FUNCTION_PRIV(mission__list, 1),
    #ifdef TESTING
    REGISTER_FUNCTION_PRIV(sys__debug, 1),
    REGISTER_FUNCTION_PRIV(mission__debug, 1),
    #endif // TESTING
    #ifdef LIVE_DEBUGGING
    REGISTER_FUNCTION_PRIV(cheats__debug, 4),
    #endif // LIVE_DEBUGGING
};

///functions that are not displayed in scripts.core
inline
std::vector<std::string> hidden_functions
{
    {"net.map"},
    {"item.manage"},
    {"ada.access"},
    {"able.help"},
    {"cash.xfer_to_caller"}, ///deprecated because we now have #os.
    {"msg.manage"}, ///deprecated due to channel.* namespace
    {"nodes.view_log"}, ///deprecated due to log.expose
};

std::map<std::string, std::vector<script_arg>> construct_core_args();
std::map<std::string, script_metadata> construct_core_metadata();

extern
std::map<std::string, std::vector<script_arg>> privileged_args;

extern
std::map<std::string, script_metadata> privileged_metadata;

std::string sec_level_of(function_priv_t func);

#ifdef USE_LOCS
inline
priv_func_info user_port_descriptor = {&user__port, 0};
#endif // USE_LOCS

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
