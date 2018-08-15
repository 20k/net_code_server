#include "rate_limiting.hpp"
#include "memory_sandbox.hpp"

bool is_script_timeout(duk_context* ctx)
{
    duk_memory_functions mem_funcs_duk;
    duk_get_memory_functions(ctx, &mem_funcs_duk);
    sandbox_data* sand_data = (sandbox_data*)mem_funcs_duk.udata;

    return sand_data->terminate_semi_gracefully || sand_data->terminate_realtime_gracefully;
}

rate_limit global_rate_limit;
