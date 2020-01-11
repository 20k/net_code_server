#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

//#include "duktape.h"
#include "scripting_api_fwrd.hpp"
#include <string>
#include <vector>

namespace js
{
    struct value_context;
}

struct shared_duk_worker_state;

struct exec_context;

int my_timeout_check(void* udata);

/*
///#db.i, r, f, u, u1, us,
duk_ret_t db_insert(duk_context* ctx);
duk_ret_t db_update(duk_context* ctx);
duk_ret_t db_find_all(duk_context* ctx);
duk_ret_t db_find_one(duk_context* ctx);

///count, first, array

duk_ret_t db_find(duk_context* ctx);
duk_ret_t db_remove(duk_context* ctx);*/

void async_pipe(js::value_context* vctx, std::string str);

void startup_state(duk_context* ctx, const std::string& caller, const std::string& script_host, const std::string& script_ending, const std::vector<std::string>& caller_stack, shared_duk_worker_state* shared_state);

void teardown_state(duk_context* ctx);

std::string get_hash_d(duk_context* ctx);

std::string get_print_str(duk_context* ctx);

void send_async_message(duk_context* ctx, const std::string& message);

std::string js_unified_force_call_data(exec_context& ctx, const std::string& data, const std::string& host);

void register_funcs(duk_context* ctx, int seclevel, const std::string& script_host, bool polyfill);

void remove_func(duk_context* ctx, const std::string& name);

#endif // SECCALLERS_HPP_INCLUDED
