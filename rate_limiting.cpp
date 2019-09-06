#include "rate_limiting.hpp"
#include "memory_sandbox.hpp"
#include <SFML/System/Sleep.hpp>
#include "mongo.hpp"

bool is_script_timeout(duk_context* ctx)
{
    duk_memory_functions mem_funcs_duk;
    duk_get_memory_functions(ctx, &mem_funcs_duk);
    sandbox_data* sand_data = (sandbox_data*)mem_funcs_duk.udata;

    return sand_data->terminate_semi_gracefully || sand_data->terminate_realtime_gracefully;
}

void handle_sleep(sandbox_data* dat)
{
    if(tls_get_holds_lock())
    {
        if(*tls_get_holds_lock() > 0)
            return;
    }

    int val = dat->sleep_for;

    if(val > 0)
    {
        /*double diff = 0;

        sf::Clock clk;

        while(diff < val)
        {
            sf::sleep(sf::milliseconds(1));

            diff += clk.restart().asMicroseconds() / 1000.;
        }

        dat->sleep_for -= floor(diff);*/

        sf::sleep(sf::milliseconds(val));
    }

    dat->sleep_for -= val;
}

rate_limit global_rate_limit;
