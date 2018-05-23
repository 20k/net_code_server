#ifndef SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED
#define SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED

#include "command_handler_state.hpp"
#include "command_handler.hpp"
#include <libncclient/socket.hpp>

struct shared_command_handler_state
{
    connection_t type = connection_type::HTTP;

    shared_command_handler_state(tcp::socket&& socket, connection_type::connection_type type) : type(type)
    {
        if(type == connection_type::HTTP)
        {
            msock = new http_socket(std::move(socket));
        }
        if(type == connection_type::WEBSOCKET)
        {
            msock = new websock_socket(std::move(socket));
        }
    }

    ~shared_command_handler_state()
    {
        delete msock;
    }

    command_handler_state state;
    socket_interface* msock = nullptr;
    global_state glob;
    int64_t my_id = 0;

    shared_data shared;

    std::atomic_bool execution_is_blocked{false};

    float live_work_units()
    {
        float blocking_script_invocation_cost = 1;
        float async_script_invocation_cost = 1;

        return blocking_script_invocation_cost * state.number_of_running_oneshot_scripts() +
               async_script_invocation_cost * state.number_of_running_realtime_scripts();
    }
};

#endif // SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED
