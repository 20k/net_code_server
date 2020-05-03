#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

//#include "duktape.h"
#include <string>
#include <vector>
#include <utility>

#include "argument_object.hpp"

struct shared_duk_worker_state;

#ifdef USE_DUKTAPE
int my_timeout_check(void* udata);
#endif // USE_DUKTAPE

#ifdef USE_QUICKJS
int interrupt_handler(JSRuntime* rt, void* udata);
#endif // USE_QUICKJS

void async_pipe(js::value_context* vctx, std::string str);

void startup_state(js::value_context& vctx, const std::string& caller, const std::string& script_host, const std::string& script_ending, const std::vector<std::string>& caller_stack, shared_duk_worker_state* shared_state);

void teardown_state(js::value_context& vctx);

void send_async_message(js::value_context& vctx, const std::string& message);

///value, message
std::pair<js::value, std::string> js_unified_force_call_data(js::value_context& vctx, const std::string& data, const std::string& host);

void register_funcs(js::value_context& vctx, int seclevel, const std::string& script_host, bool polyfill);

#endif // SECCALLERS_HPP_INCLUDED
