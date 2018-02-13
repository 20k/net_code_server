#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

#include "script_util.hpp"
#include "mongo.hpp"

inline
std::string get_caller(duk_context* ctx)
{
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "caller");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

///#db.i, r, f, u, u1, us,

///ALARM
///NEED TO REGISTER SCRIPT OWNER AND USE THEIR DB
static
duk_ret_t db_insert(duk_context* ctx)
{
    ///FIX ME IMMEDIATELY
    mongo_context* mongo_ctx = get_global_mongo_context();
    mongo_ctx->change_collection(get_caller(ctx));

    std::string json = duk_json_encode(ctx, -1);

    mongo_ctx->insert_json_1(json);

    //mongo_ctx.insert_test_data();

    //mongo_ctx.ping();

    std::cout << "json " << json << std::endl;

    return 1;
}

inline
void startup_state(duk_context* ctx, const std::string& caller)
{
    duk_push_global_stash(ctx);
    duk_push_string(ctx, "");
    duk_put_prop_string(ctx, -2, "HASH_D");

    duk_push_string(ctx, caller.c_str());
    duk_put_prop_string(ctx, -2, "caller");

    duk_pop_n(ctx, 1);
}

static
duk_ret_t hash_d(duk_context* ctx)
{
    std::string str = duk_json_encode(ctx, -1);
    //duk_pop(ctx);

    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "HASH_D");

    std::string fstr = duk_safe_to_string(ctx, -1);

    fstr += str;

    duk_pop_n(ctx, 1);

    duk_push_string(ctx, fstr.c_str());
    duk_put_prop_string(ctx, -2, "HASH_D");

    duk_pop_n(ctx, 2);

    return 0;
}

///need to pass context object into function
static
duk_ret_t js_call(duk_context* ctx)
{
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "TO_CALL_INTERNAL_XXX");

    std::string str = duk_require_string(ctx, -1);

    duk_pop_n(ctx, 2);

    std::string load = parse_script(get_script_from_name_string(base_scripts_string, str));

    //std::cout << load << std::endl;

    stack_duk sd;
    sd.ctx = ctx;

    ///so.
    ///need to pass context and args along
    compile_and_call(sd, load, true, get_caller(ctx));

    ///oh boy
    ///ok, need to read in the file

    return 1;
}

///so ideally this would provide validation
///pass through context and set appropriately
///and modify args
inline
duk_ret_t sl_call(duk_context* ctx, int sl)
{
    std::string str = duk_require_string(ctx, -1);

    duk_push_global_stash(ctx);
    duk_push_string(ctx, str.c_str());
    duk_put_prop_string(ctx, -2, "TO_CALL_INTERNAL_XXX");

    duk_pop_n(ctx, 2);

    duk_push_c_function(ctx, &js_call, 1);

    return 1;
}

static duk_ret_t fs_call(duk_context *ctx)
{
    return sl_call(ctx, 4);
}

static duk_ret_t hs_call(duk_context *ctx)
{
    return sl_call(ctx, 3);
}

static duk_ret_t ms_call(duk_context *ctx)
{
    return sl_call(ctx, 2);
}

static duk_ret_t ls_call(duk_context *ctx)
{
    return sl_call(ctx, 1);
}

static duk_ret_t ns_call(duk_context *ctx)
{
    return sl_call(ctx, 0);
}

void register_funcs(duk_context* ctx)
{
    inject_c_function(ctx, fs_call, "fs_call", 1);
    inject_c_function(ctx, hs_call, "hs_call", 1);
    inject_c_function(ctx, ms_call, "ms_call", 1);
    inject_c_function(ctx, ls_call, "ls_call", 1);
    inject_c_function(ctx, ns_call, "ns_call", 1);

    inject_c_function(ctx, hash_d, "hash_d", 1);

    inject_c_function(ctx, db_insert, "db_insert", 1);
    /*inject_c_function(ctx, hash_d, "hash_d", 1);
    inject_c_function(ctx, hash_d, "hash_d", 1);
    inject_c_function(ctx, hash_d, "hash_d", 1);
    inject_c_function(ctx, hash_d, "hash_d", 1);
    inject_c_function(ctx, hash_d, "hash_d", 1);*/
}

#endif // SECCALLERS_HPP_INCLUDED
