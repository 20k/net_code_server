#ifndef PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
#define PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED

#include "user.hpp"

using function_priv_t = duk_ret_t (*)(duk_context*, int);

inline
duk_ret_t accts_balance(duk_context* ctx, int sl)
{
    //mongo_context* mongo_ctx = get_global_mongo_user_info_context();
    //mongo_ctx->change_collection(get_caller(ctx));

    user usr;
    usr.load_from_db(get_caller(ctx));

    duk_push_number(ctx, usr.cash);

    return 1;
}

#define REGISTER_FUNCTION_PRIV(x) {#x, &x}

inline static
std::map<std::string, function_priv_t> privileged_functions =
{
    REGISTER_FUNCTION_PRIV(accts_balance),
};

#endif // PRIVILEGED_CORE_SCRIPTS_HPP_INCLUDED
