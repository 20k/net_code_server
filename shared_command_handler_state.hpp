#ifndef SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED
#define SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED

#include "command_handler_state.hpp"
#include "command_handler.hpp"
#include <libncclient/socket.hpp>

struct shared_command_handler_state
{
    shared_command_handler_state(tcp::socket&& socket, connection_type::connection_type type)
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
};

#endif // SHARED_COMMAND_HANDLER_STATE_HPP_INCLUDED
