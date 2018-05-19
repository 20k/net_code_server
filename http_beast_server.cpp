#include <libncclient/socket.hpp>

#include "http_beast_server.hpp"
#include "non_user_task_thread.hpp"
#include "shared_data.hpp"
#include "logging.hpp"
#include <libncclient/nc_util.hpp>
#include <json/json.hpp>

//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, synchronous
//
//------------------------------------------------------------------------------

//#define HOST_IP "77.96.132.101"
//#define HOST_IP "0.0.0.0"

#ifdef LOCAL_IP
#define HOST_IP "127.0.0.1"
#endif // LOCAL_IP

#ifdef EXTERN_IP
#define HOST_IP "0.0.0.0"
#endif // EXTERN_IP

#ifdef EXTERN_IP
#define HOST_PORT 6750
#define HOST_WEBSOCKET_PORT 6760
#endif // EXTERN_IP

#ifdef LOCAL_IP
#define HOST_PORT 6751
#define HOST_WEBSOCKET_PORT 6761
#endif // LOCAL_IP


//#include <boost/beast/core.hpp>
//#include <boost/beast/websocket.hpp>
//#include <boost/beast/http.hpp>
//#include <boost/beast/version.hpp>
//#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "command_handler.hpp"

namespace connection_type
{
    enum connection_type
    {
        WEBSOCKET,
        HTTP
    };
}

using connection_t = connection_type::connection_type;

void async_command_handler(shared_data& shared, command_handler_state& state, std::deque<std::string>& shared_queue, std::mutex& shared_lock,
                           global_state& glob, int64_t my_id)
{
    while(1)
    {
        if(shared.should_terminate)
            break;

        std::string to_exec = "";
        bool found_command = false;

        {
            std::lock_guard lg(shared_lock);

            if(shared_queue.size() > 0)
            {
                to_exec = shared_queue.front();
                shared_queue.pop_front();
                found_command = true;
            }
        }

        if(to_exec == "" && !found_command)
        {
            Sleep(5);
            continue;
        }

        std::string to_pipe = handle_command(state, to_exec, glob, my_id, shared);
        shared.add_back_write(to_pipe);

        Sleep(5);
    }

    std::cout << "shutdown async" << std::endl;

    shared.termination_count++;
}

bool handle_termination_shortcircuit(command_handler_state& state, const std::string& str, shared_data& shared, global_state& glob, int64_t my_id)
{
    std::string tstr = "client_terminate_scripts ";

    if(starts_with(str, tstr))
    {
        std::string to_parse(str.begin() + tstr.size(), str.end());

        try
        {
            using nlohmann::json;

            json j = json::parse(to_parse);

            int id = j["id"];

            if(id <= -1)
            {
                state.should_terminate_any_realtime = true;
            }
            else
            {
                std::lock_guard guard(state.lock);

                if(state.should_terminate_realtime.size() > 100)
                    state.should_terminate_realtime.clear();

                state.should_terminate_realtime[id] = true;
            }

            return true;
        }
        catch(...)
        {
            return true;
        }

        return true;
    }

    std::string kstr = "client_script_keystrokes ";

    if(starts_with(str, kstr))
    {
        try
        {
            using nlohmann::json;

            std::string to_parse(str.begin() + kstr.size(), str.end());

            json j = json::parse(to_parse);

            int id = j["id"];
            std::string str = j["keys"];

            if(str.size() > 10)
                return true;

            {
                std::lock_guard guard(state.lock);

                state.unprocessed_keystrokes[id] += str;

                while(state.unprocessed_keystrokes[id].size() > 200)
                {
                    state.unprocessed_keystrokes[id].erase(state.unprocessed_keystrokes[id].begin());
                }
            }

            return true;
        }
        catch(...)
        {
            return true;
        }
    }

    if(str == "client_poll" || str == "client_poll_json")
    {
        std::string out = handle_command(state, str, glob, my_id, shared);

        shared.add_back_write(out);

        return true;
    }

    return false;
}

// Handles an HTTP server connection
void read_queue(socket_interface& socket,
                global_state& glob,
                int64_t my_id,
                shared_data& shared,
                std::deque<std::string>& shared_queue,
                std::mutex& shared_lock,
                command_handler_state& state,
                connection_t conn_type)
{
    std::thread(async_command_handler, std::ref(shared), std::ref(state), std::ref(shared_queue), std::ref(shared_lock),
                std::ref(glob), my_id).detach();

    boost::system::error_code ec;

    try
    {
        while(1)
        {
            if(shared.should_terminate)
                break;

            Sleep(4);

            //if(socket.available() > 0)
            {
                if(socket.read(ec))
                    break;

                std::string next_command = socket.get_read();

                lg::log(next_command);

                if(handle_termination_shortcircuit(state, next_command, shared, glob, my_id))
                   continue;

                int len;

                bool rate_hit = true;

                {
                    std::lock_guard lg(shared_lock);

                    len = shared_queue.size();

                    std::string str = next_command;

                    if(len <= 20 || starts_with(str, "client_poll"))
                    {
                        shared_queue.push_back(next_command);
                        rate_hit = false;
                    }
                }

                if(rate_hit)
                {
                    lg::log("hit rate limit");
                    shared.add_back_write("command Hit rate limit (read_queue)");
                }
            }
        }
    }
    catch(...)
    {

    }

    state.should_terminate_any_realtime = true;

    shared.should_terminate = true;

    socket.shutdown();

    std::cout << "shutdown read" << std::endl;

    shared.termination_count++;
}

void write_queue(socket_interface& socket,
                global_state& glob,
                int64_t my_id,
                shared_data& shared,
                connection_t conn_type)
{
    try
    {
        while(1)
        {
            if(shared.should_terminate)
                break;

            if(shared.has_front_write())
            {
                std::string next_command = shared.get_front_write();

                //if(next_command != "")
                //    printf("sending test write\n");

                if(next_command == "" && conn_type == connection_type::WEBSOCKET)
                    continue;

                if(socket.write(next_command))
                    break;
            }
            else
            {
                Sleep(1);
            }

            if(!socket.is_open())
                break;
        }
    }
    catch(...)
    {

    }

    shared.should_terminate = true;

    shared.termination_count++;

    std::cout << "shutdown write" << std::endl;
}

void thread_session(
    tcp::socket&& socket,
    global_state& glob,
    int64_t my_id,
    connection_t conn_type)
{
    shared_data shared;

    std::deque<std::string> shared_queue;
    std::mutex shared_lock;
    command_handler_state state;


    global_shared_data* store = fetch_global_shared_data();
    store->add(&shared);

    socket_interface* msock = nullptr;

    if(conn_type == connection_type::HTTP)
    {
        msock = new http_socket(std::move(socket));
    }
    if(conn_type == connection_type::WEBSOCKET)
    {
        msock = new websock_socket(std::move(socket));
    }

    std::thread(read_queue, std::ref(*msock), std::ref(glob), my_id, std::ref(shared),
                std::ref(shared_queue), std::ref(shared_lock), std::ref(state), conn_type).detach();
    std::thread(write_queue, std::ref(*msock), std::ref(glob), my_id, std::ref(shared), conn_type).detach();

    ///3rd thread is the js exec context
    while(shared.termination_count != 3 || state.number_of_realtime_scripts_terminated != state.number_of_realtime_scripts)
    {
        if(state.number_of_realtime_scripts_terminated == state.number_of_realtime_scripts)
        {
            state.should_terminate_any_realtime = false;
        }

        Sleep(500);
    }

    {
        std::lock_guard guard(store->lock);

        for(int i=0; i < (int)store->data.size(); i++)
        {
            if(store->data[i] == &shared)
            {
                store->data.erase(store->data.begin() + i);
                break;
            }
        }
    }

    Sleep(50);

    delete msock;

    std::cout << "shutdown session" << std::endl;
}

void session_wrapper(tcp::socket&& socket,
                     std::string const& doc_root,
                     global_state& glob,
                     int64_t my_id,
                     connection_t conn_type)
{
    try
    {
        thread_session(std::move(socket), glob, my_id, conn_type);
    }
    catch(...)
    {

    }
}

void http_test_server()
{
    try
    {
        auto const address = boost::asio::ip::make_address(HOST_IP);
        auto const port = static_cast<unsigned short>(HOST_PORT);
        std::string const doc_root = "./doc_root";

        // The io_context is required for all I/O
        boost::asio::io_context ioc{2};

        global_state glob;

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};
        for(;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            int id = glob.global_id++;

            // Launch the session, transferring ownership of the socket
            std::thread(
                session_wrapper,
                std::move(socket),
                doc_root,
                std::ref(glob),
                id,
                connection_type::HTTP).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "HTTP Error: " << e.what() << std::endl;
        //return EXIT_FAILURE;
    }
}

void websocket_test_server()
{
    try
    {
        auto const address = boost::asio::ip::make_address(HOST_IP);
        auto const port = static_cast<unsigned short>(HOST_WEBSOCKET_PORT);
        std::string const doc_root = "./doc_root";

        // The io_context is required for all I/O
        boost::asio::io_context ioc{2};

        global_state glob;

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};
        for(;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            int id = glob.global_id++;

            // Launch the session, transferring ownership of the socket
            std::thread(
                session_wrapper,
                std::move(socket),
                doc_root,
                std::ref(glob),
                id,
                connection_type::WEBSOCKET).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Websocket Error: " << e.what() << std::endl;
        //return EXIT_FAILURE;
    }
}

void http_test_run()
{
    //std::thread{std::bind(&http_test_server, &req)}.detach();

    start_non_user_task_thread();

    std::thread(http_test_server).detach();
    //std::thread(websocket_test_server).detach();

    std::thread(websocket_test_server).detach();

    //http_test_server();
}
