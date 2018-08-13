#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

//#include "duktape.h"
#include "scripting_api_fwrd.hpp"
#include <string>
#include <vector>

struct shared_duk_worker_state;

int my_timeout_check(void* udata);

duk_ret_t native_print(duk_context *ctx);

duk_ret_t timeout_yield(duk_context* ctx);

///#db.i, r, f, u, u1, us,
duk_ret_t db_insert(duk_context* ctx);

duk_ret_t db_update(duk_context* ctx);

duk_ret_t db_find_all(duk_context* ctx);

duk_ret_t db_find_one(duk_context* ctx);

///count, first, array

duk_ret_t db_find(duk_context* ctx);

duk_ret_t db_remove(duk_context* ctx);

duk_ret_t set_is_realtime_script(duk_context* ctx);
duk_ret_t async_pipe(duk_context* ctx);

duk_ret_t is_realtime_script(duk_context* ctx);
duk_ret_t set_close_window_on_exit(duk_context* ctx);
duk_ret_t set_start_window_size(duk_context* ctx);
duk_ret_t set_realtime_framerate_limit(duk_context* ctx);
duk_ret_t is_key_down(duk_context* ctx);
duk_ret_t mouse_get_position(duk_context* ctx);

void startup_state(duk_context* ctx, const std::string& caller, const std::string& script_host, const std::string& script_ending, const std::vector<std::string>& caller_stack, shared_duk_worker_state* shared_state);

void teardown_state(duk_context* ctx);

duk_ret_t get_string_col(duk_context* ctx);

duk_ret_t terminate_realtime(duk_context* ctx);

duk_ret_t hash_d(duk_context* ctx);

std::string get_hash_d(duk_context* ctx);

std::string get_print_str(duk_context* ctx);

void send_async_message(duk_context* ctx, const std::string& message);

duk_ret_t js_call(duk_context* ctx, int sl);

std::string js_unified_force_call_data(duk_context* ctx, const std::string& data, const std::string& host);

void register_funcs(duk_context* ctx, int seclevel, const std::string& script_host);

duk_ret_t err(duk_context* ctx);
std::string add_freeze(const std::string& name);

void remove_func(duk_context* ctx, const std::string& name);

#endif // SECCALLERS_HPP_INCLUDED
