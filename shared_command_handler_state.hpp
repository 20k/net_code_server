#ifndef SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED
#define SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED

#include "command_handler_state.hpp"
#include "command_handler.hpp"
#include <libncclient/socket.hpp>

struct shared_command_handler_state
{
    command_handler_state state;
    int64_t my_id = 0;

    shared_data shared;

    std::atomic_bool execution_is_blocked{false};
    std::atomic_bool execution_requested{false};

    float live_work_units()
    {
        float blocking_script_invocation_cost = 1;
        float async_script_invocation_cost = 1;

        float value = blocking_script_invocation_cost * state.number_of_running_oneshot_scripts() +
                      async_script_invocation_cost * state.number_of_running_realtime_work_units();

        if(value < 1)
            return 1;
        else
            return value;
    }
};

#endif // SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED
