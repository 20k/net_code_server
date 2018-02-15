#ifndef PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
#define PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED

#include "user.hpp"
#include "duk_object_functions.hpp"

using function_priv_t = duk_ret_t (*)(duk_context*, int);

bool can_run(int csec_level, int maximum_sec)
{
    return csec_level <= maximum_sec;
}

void push_error(duk_context* ctx, const std::string& msg)
{
    push_dukobject(ctx, "ok", false, "msg", msg);
}

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
duk_ret_t accts__balance(duk_context* ctx, int sl)
{
    SL_GUARD(3);

    user usr;
    usr.load_from_db(get_caller(ctx));

    std::string cash_string = std::to_string((int64_t)usr.cash);

    push_duk_val(ctx, cash_string);

    return 1;
}

inline
duk_ret_t scripts__get_level(duk_context* ctx, int sl)
{
    SL_GUARD(4);

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

    script_info script;
    script.load(str);

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

    //std::replace(in.begin(), in.end(), '__', '');

    in.erase(std::remove(in.begin(), in.end(), '$'), in.end());

    return in;
}

#define REGISTER_FUNCTION_PRIV(x, y) {parse_function_hack(#x), {&x, y}}

inline
std::map<std::string, priv_func_info> privileged_functions
{
    REGISTER_FUNCTION_PRIV(accts__balance, 3),
    REGISTER_FUNCTION_PRIV(scripts__get_level, 4),
};

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
