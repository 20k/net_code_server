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

///so say this is midsec
///we can run if the sl is midsec or lower
///lower sls are less secure

///hmm. Maybe we want to keep sls somewhere which is dynamically editable like global properties in the db
///cache the calls, and like, refresh the cache every 100 calls or something
inline
duk_ret_t accts_balance(duk_context* ctx, int sl)
{
    SL_GUARD(3);

    user usr;
    usr.load_from_db(get_caller(ctx));

    std::string cash_string = std::to_string((int64_t)usr.cash);

    push_duk_val(ctx, cash_string);

    return 1;
}

#define REGISTER_FUNCTION_PRIV(x) {#x, &x}

inline static
std::map<std::string, function_priv_t> privileged_functions =
{
    REGISTER_FUNCTION_PRIV(accts_balance),
};

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
