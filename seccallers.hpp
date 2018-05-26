#ifndef SECCALLERS_HPP_INCLUDED
#define SECCALLERS_HPP_INCLUDED

#include "duk_object_functions.hpp"

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
duk_ret_t is_key_down(duk_context* ctx);
duk_ret_t mouse_get_position(duk_context* ctx);

void startup_state(duk_context* ctx, const std::string& caller, const std::string& script_host, const std::string& script_ending, const std::vector<std::string>& caller_stack, shared_duk_worker_state* shared_state);

void teardown_state(duk_context* ctx);

duk_ret_t get_string_col(duk_context* ctx);

duk_ret_t terminate_realtime(duk_context* ctx);

duk_ret_t hash_d(duk_context* ctx);

std::string get_hash_d(duk_context* ctx);

std::string get_print_str(duk_context* ctx);

duk_ret_t js_call(duk_context* ctx, int sl);

std::string js_unified_force_call_data(duk_context* ctx, const std::string& data, const std::string& host);

void register_funcs(duk_context* ctx, int seclevel);

template<int N>
static
duk_ret_t jxs_call(duk_context* ctx)
{
    int current_seclevel = get_global_int(ctx, "last_seclevel");

    duk_ret_t ret = js_call(ctx, N);

    set_global_int(ctx, "last_seclevel", current_seclevel);

    register_funcs(ctx, current_seclevel);

    return ret;
}

duk_ret_t err(duk_context* ctx);

///so ideally this would provide validation
///pass through context and set appropriately
///and modify args
template<int N>
inline
duk_ret_t sl_call(duk_context* ctx)
{
    static_assert(N >= 0 && N <= 4);

    std::string str = duk_require_string(ctx, -1);

    duk_push_c_function(ctx, &jxs_call<N>, 1);

    put_duk_keyvalue(ctx, "FUNCTION_NAME", str);
    put_duk_keyvalue(ctx, "call", err);

    freeze_duk(ctx);

    return 1;
}

std::string add_freeze(const std::string& name);

void do_freeze(duk_context* ctx, const std::string& name, std::string& script_accumulate);

inline
void fully_freeze_recurse(duk_context* ctx, std::string& accum){}

template<typename U, typename... T>
inline
void fully_freeze_recurse(duk_context* ctx, std::string& accum, const U& arg, T&&... args)
{
    do_freeze(ctx, arg, accum);

    fully_freeze_recurse(ctx, accum, args...);
}

template<typename... T>
inline
void fully_freeze(duk_context* ctx, T&&... args)
{
    std::string get_global = "var global = new Function(\'return this;\')();";

    std::string freeze_script = get_global + "\nfunction deepFreeze(o) {\n"
          "Object.freeze(o);\n"

          "Object.getOwnPropertyNames(o).forEach(function(prop) {\n"
            "if (o.hasOwnProperty(prop)\n"
            "&& o[prop] !== null\n"
            "&& (typeof o[prop] === \"object\" || typeof o[prop] === \"function\")\n"
            "&& !Object.isFrozen(o[prop])) {\n"
                "deepFreeze(o[prop]);\n"
              "}\n"
          "});\n"

         "return o;\n"
        "};\n\n";

    fully_freeze_recurse(ctx, freeze_script, args...);

    duk_int_t res = duk_peval_string(ctx, freeze_script.c_str());

    if(res != 0)
    {
        std::string err = duk_safe_to_string(ctx, -1);

        printf("freeze eval failed: %s\n", err.c_str());
    }
    else
    {
        duk_pop(ctx);
    }
}

void remove_func(duk_context* ctx, const std::string& name);

#endif // SECCALLERS_HPP_INCLUDED
