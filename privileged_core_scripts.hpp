#ifndef PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
#define PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED

#include "user.hpp"
#include "memory_sandbox.hpp"
#include "rate_limiting.hpp"
#include "auth.hpp"
#include "item.hpp"
#include <libncclient/nc_util.hpp>
#include "logging.hpp"
#include "unified_scripts.hpp"

#include <vec/vec.hpp>

#define USE_SECRET_CONTENT
#ifdef USE_SECRET_CONTENT
#include <secret/secret.hpp>
#include <secret/node.hpp>
#include <secret/npc_manager.hpp>
#endif // USE_SECRET_CONTENT

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

///so say this is midsec
///we can run if the sl is midsec or lower
///lower sls are less secure

///hmm. Maybe we want to keep sls somewhere which is dynamically editable like global properties in the db
///cache the calls, and like, refresh the cache every 100 calls or something

duk_ret_t cash__balance(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t scripts__get_level(priv_context& priv_ctx, duk_context* ctx, int sl);

std::string format_pretty_names(const std::vector<std::string>& names);

duk_ret_t scripts__me(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t scripts__public(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t cash_internal_xfer(duk_context* ctx, const std::string& from, const std::string& to, double amount);
///TODO: TRANSACTION HISTORY

duk_ret_t cash__xfer_to(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t cash__xfer_to_caller(priv_context& priv_ctx, duk_context* ctx, int sl);

///this is only valid currently, will need to expand to hardcode in certain folders

duk_ret_t scripts__core(priv_context& priv_ctx, duk_context* ctx, int sl);

size_t get_wall_time();
double get_wall_time_s();

bool user_in_channel(mongo_lock_proxy& mongo_ctx, const std::string& username, const std::string& channel);

bool is_valid_channel_name(const std::string& in);

duk_ret_t msg__manage(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t msg__send(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t msg__tell(priv_context& priv_ctx, duk_context* ctx, int sl);

void create_notification(duk_context* ctx, const std::string& to, const std::string& notif_msg);
void create_xfer_notif(duk_context* ctx, const std::string& xfer_from, const std::string& xfer_to, double amount);
void create_xfer_item_notif(duk_context* ctx, const std::string& xfer_from, const std::string& xfer_to, const std::string& item_name);
void create_destroy_item_notif(duk_context* ctx, const std::string& to, const std::string& item_name);

///formats time
std::string format_time(const std::string& in);
std::string prettify_chat_strings(std::vector<nlohmann::json>& found, bool use_channels = true);

duk_ret_t msg__recent(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t users__me(priv_context& priv_ctx, duk_context* ctx, int sl);

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

void push_internal_items_view(duk_context* ctx, int pretty, int full, user_nodes& nodes, user& found_user, std::string preamble = "");

duk_ret_t item__cull(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t item__manage(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t push_xfer_item_with_logs(duk_context* ctx, int item_idx, const std::string& from, const std::string& to);

duk_ret_t item__xfer_to(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t item__bundle_script(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t item__register_bundle(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t item__configure_on_breach(priv_context& priv_ctx, duk_context* ctx, int sl);
#ifdef TESTING

duk_ret_t item__create(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // TESTING


duk_ret_t cash__expose(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t item__expose(priv_context& priv_ctx, duk_context* ctx, int sl);

///handles confirm:true for cash
duk_ret_t handle_confirmed(duk_context* ctx, bool confirm, const std::string& username, double price);

///have item__steal reset internal node structure

duk_ret_t item__steal(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t cash__steal(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t nodes__view_log(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t hack_internal(priv_context& priv_ctx, duk_context* ctx, const std::string& name_of_person_being_attacked);

#ifdef USE_LOCS
duk_ret_t user__port(priv_context& priv_ctx, duk_context* ctx, int sl);
#endif // USE_LOCS


duk_ret_t net__hack(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t nodes__manage(priv_context& priv_ctx, duk_context* ctx, int sl);

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

duk_ret_t net__view(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__map(priv_context& priv_ctx, duk_context* ctx, int sl);
//duk_ret_t net__links(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__access(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__switch(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__move(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__path(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t net__modify(priv_context& priv_ctx, duk_context* ctx, int sl);

duk_ret_t gal__map(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t gal__list(priv_context& priv_ctx, duk_context* ctx, int sl);


#ifdef TESTING
duk_ret_t sys__map(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t sys__view(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t sys__move(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t sys__access(priv_context& priv_ctx, duk_context* ctx, int sl);
duk_ret_t sys__debug(priv_context& priv_ctx, duk_context* ctx, int sl);
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
    REGISTER_FUNCTION_PRIV(cash__expose, 4),
    REGISTER_FUNCTION_PRIV(cash__xfer_to, 2),
    REGISTER_FUNCTION_PRIV(cash__xfer_to_caller, 4),
    REGISTER_FUNCTION_PRIV(cash__steal, 4),
    REGISTER_FUNCTION_PRIV(scripts__get_level, 4),
    REGISTER_FUNCTION_PRIV(scripts__core, 4),
    REGISTER_FUNCTION_PRIV(scripts__me, 2),
    REGISTER_FUNCTION_PRIV(scripts__public, 4),
    REGISTER_FUNCTION_PRIV(msg__manage, 3),
    REGISTER_FUNCTION_PRIV(msg__send, 3),
    REGISTER_FUNCTION_PRIV(msg__tell, 3),
    REGISTER_FUNCTION_PRIV(msg__recent, 2),
    REGISTER_FUNCTION_PRIV(users__me, 0),
    #ifdef TESTING
    REGISTER_FUNCTION_PRIV(item__create, 0),
    #endif // TESTING
    REGISTER_FUNCTION_PRIV(item__steal, 4),
    REGISTER_FUNCTION_PRIV(item__expose, 4),
    //REGISTER_FUNCTION_PRIV(sys__disown_upg, 0),
    //REGISTER_FUNCTION_PRIV(sys__xfer_upgrade_uid, 0),
    REGISTER_FUNCTION_PRIV(item__xfer_to, 1),
    REGISTER_FUNCTION_PRIV(item__manage, 2),
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
    REGISTER_FUNCTION_PRIV(net__view, 1),
    REGISTER_FUNCTION_PRIV(net__map, 1),
    REGISTER_FUNCTION_PRIV(net__hack, 0),
    REGISTER_FUNCTION_PRIV(net__access, 0),
    REGISTER_FUNCTION_PRIV(net__switch, 0),
    REGISTER_FUNCTION_PRIV(net__move, 0),
    REGISTER_FUNCTION_PRIV(net__path, 0),
    REGISTER_FUNCTION_PRIV(net__modify, 0),
    REGISTER_FUNCTION_PRIV(gal__map, 1),
    REGISTER_FUNCTION_PRIV(gal__list, 4),
    #ifdef TESTING
    REGISTER_FUNCTION_PRIV(cheats__arm, 4),
    REGISTER_FUNCTION_PRIV(cheats__salvage, 4),
    REGISTER_FUNCTION_PRIV(cheats__give, 4),
    REGISTER_FUNCTION_PRIV(cheats__task, 4),
    REGISTER_FUNCTION_PRIV(cheats__disconnect, 4),
    REGISTER_FUNCTION_PRIV(cheats__unlink, 4),
    REGISTER_FUNCTION_PRIV(cheats__testloot, 4),
    #endif // TESTING
    #ifdef TESTING
    REGISTER_FUNCTION_PRIV(sys__map, 1),
    REGISTER_FUNCTION_PRIV(sys__view, 1),
    REGISTER_FUNCTION_PRIV(sys__move, 0),
    REGISTER_FUNCTION_PRIV(sys__access, 0),
    REGISTER_FUNCTION_PRIV(sys__debug, 1),
    #endif // TESTING
    #ifdef LIVE_DEBUGGING
    REGISTER_FUNCTION_PRIV(cheats__debug, 4),
    #endif // LIVE_DEBUGGING
};

std::map<std::string, std::vector<script_arg>> construct_core_args();

extern
std::map<std::string, std::vector<script_arg>> privileged_args;

#ifdef USE_LOCS
inline
priv_func_info user_port_descriptor = {&user__port, 0};
#endif // USE_LOCS

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
