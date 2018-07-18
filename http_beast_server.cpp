#include <libncclient/socket.hpp>

#include "http_beast_server.hpp"
#include "non_user_task_thread.hpp"
#include "shared_data.hpp"
#include "logging.hpp"
#include <libncclient/nc_util.hpp>
#include <json/json.hpp>
#include "shared_command_handler_state.hpp"

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
#define HOST_WEBSOCKET_SSL_PORT 6770
#define HOST_WEBSOCKET_SSL_PORT_2 6780
#endif // EXTERN_IP

#ifdef LOCAL_IP
#define HOST_PORT 6751
#define HOST_WEBSOCKET_PORT 6761
#define HOST_WEBSOCKET_SSL_PORT 6771
#define HOST_WEBSOCKET_SSL_PORT_2 6781
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

void async_command_handler(std::shared_ptr<shared_command_handler_state> all_shared, std::deque<std::string>& shared_queue, std::mutex& shared_lock)
{
    lg::log("async_queue\n");

    while(1)
    {
        if(all_shared->shared.should_terminate)
            break;

        std::string to_exec = "";
        bool found_command = false;

        {
            safe_lock_guard lg(shared_lock);

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



        //std::string to_pipe = handle_command(all_shared, to_exec);
        //all_shared->shared.add_back_write(to_pipe);

        while(all_shared->execution_is_blocked)
        {
            Sleep(5);
        }

        async_handle_command(all_shared, to_exec);

        Sleep(5);
    }

    std::cout << "shutdown async" << std::endl;

    all_shared->shared.termination_count++;
}

std::vector<std::string> sanitise_input_vec(std::vector<std::string> vec)
{
    if(vec.size() > 10)
        vec.resize(10);

    for(auto& i : vec)
    {
        if(i.size() > 10)
            i.resize(10);
    }

    return vec;
}

bool handle_termination_shortcircuit(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str)
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
                all_shared->state.should_terminate_any_realtime = true;
            }
            else
            {
                safe_lock_guard guard(all_shared->state.lock);

                if(all_shared->state.should_terminate_realtime.size() > 100)
                    all_shared->state.should_terminate_realtime.clear();

                all_shared->state.should_terminate_realtime[id] = true;
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

            //std::cout << "parsed " << to_parse << std::endl;

            json j = json::parse(to_parse);

            int id = j["id"];

            ///todo
            ///keystroke special funtime
            ///create a push vector function
            ///need to also have an internal map of key state
            ///assume keys are not pressed when we get them the first time, dont do any magic

            if(j.find("input_keys") != j.end())
            {
                try
                {
                    std::vector<std::string> str = j["input_keys"];
                    str = sanitise_input_vec(str);

                    //for(auto& i : str)
                    //    std::cout << "keystroke " << i << "\n";

                    {
                        safe_lock_guard guard(all_shared->state.lock);

                        for(auto& i : str)
                            all_shared->state.unprocessed_keystrokes[id].push_back(i);

                        while(all_shared->state.unprocessed_keystrokes[id].size() > 200)
                        {
                            all_shared->state.unprocessed_keystrokes[id].erase(all_shared->state.unprocessed_keystrokes[id].begin());
                        }
                    }
                }
                catch(...){}
            }

            if(j.find("pressed_keys") != j.end())
            {
                //std::cout << "presser\n";

                try
                {
                    std::vector<std::string> str = j["pressed_keys"];
                    str = sanitise_input_vec(str);

                    /*for(auto& i : str)
                    {
                        std::cout << " dfdf " << i << std::endl;
                    }*/

                    for(auto& i : str)
                    {
                        //std::cout << "Pressed " << i << std::endl;

                        all_shared->state.set_key_state(id, i, true);
                    }
                }
                catch(...){}
            }

            if(j.find("released_keys") != j.end())
            {
                try
                {
                    std::vector<std::string> str = j["released_keys"];
                    str = sanitise_input_vec(str);

                    for(auto& i : str)
                    {
                        all_shared->state.set_key_state(id, i, false);
                    }
                }
                catch(...){}
            }

            return true;
        }
        catch(...)
        {
            return true;
        }
    }

    std::string istr = "client_script_info ";

    if(starts_with(str, istr))
    {
        try
        {
            using nlohmann::json;

            std::string to_parse(str.begin() + istr.size(), str.end());

            json j = json::parse(to_parse);

            int id = j["id"];

            if(id < 0)
                return true;

            int width = j["width"];
            int height = j["height"];

            all_shared->state.set_width_height(id, width, height);

            return true;
        }
        catch(...)
        {
            return true;
        }
    }

    std::string mstr = "client_script_mouseinput ";

    if(starts_with(str, mstr))
    {
        try
        {
            using nlohmann::json;

            std::string to_parse(str.begin() + mstr.size(), str.end());

            json j = json::parse(to_parse);

            int id = j["id"];

            if(id < 0)
                return true;

            float mouse_x = j["mouse_x"];
            float mouse_y = j["mouse_y"];

            float mousewheel_x = j["mousewheel_x"];
            float mousewheel_y = j["mousewheel_y"];

            all_shared->state.add_mouse_state(id, {mouse_x, mouse_y}, {mousewheel_x, mousewheel_y});

            return true;
        }
        catch(...)
        {
            return true;
        }
    }

    if(str == "client_poll" || str == "client_poll_json")
    {
        std::string out = handle_command(all_shared, str);

        all_shared->shared.add_back_write(out);

        return true;
    }

    return false;
}

// Handles an HTTP server connection
void read_queue(std::shared_ptr<shared_command_handler_state> all_shared,
                std::deque<std::string>& shared_queue,
                std::mutex& shared_lock)
{
    std::thread(async_command_handler, all_shared, std::ref(shared_queue), std::ref(shared_lock)).detach();

    boost::system::error_code ec;

    lg::log("read_queue\n");

    try
    {
        while(1)
        {
            if(all_shared->shared.should_terminate)
                break;

            Sleep(2);

            if(all_shared->msock->available() > 0)
            {
                if(all_shared->msock->read(ec))
                    break;

                std::string next_command = all_shared->msock->get_read();

                if(next_command.size() > 200000)
                    continue;

                lg::log(next_command);

                if(handle_termination_shortcircuit(all_shared, next_command))
                    continue;

                int len;

                bool rate_hit = true;

                {
                    safe_lock_guard lg(shared_lock);

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
                    all_shared->shared.add_back_write("command Hit rate limit (read_queue)");
                }
            }
        }
    }
    catch(...)
    {

    }

    all_shared->state.should_terminate_any_realtime = true;

    all_shared->shared.should_terminate = true;

    all_shared->msock->shutdown();

    std::cout << "shutdown read" << std::endl;

    all_shared->shared.termination_count++;
}

void write_queue(std::shared_ptr<shared_command_handler_state> all_shared)
{
    lg::log("write_queue\n");

    try
    {
        while(1)
        {
            if(all_shared->shared.should_terminate)
                break;

            if(all_shared->msock->timed_out())
                break;

            if(all_shared->shared.has_front_write())
            {
                std::string next_command = all_shared->shared.get_front_write();

                //if(next_command != "")
                //    printf("sending test write\n");

                if(next_command == "" && all_shared->type!= connection_type::HTTP)
                    continue;

                if(next_command.size() > MAX_MESSAGE_SIZE)
                {
                    next_command.resize(MAX_MESSAGE_SIZE);

                    next_command += " [Truncated, > " + std::to_string(MAX_MESSAGE_SIZE) + "]";
                }

                if(all_shared->msock->write(next_command))
                    break;
            }
            else
            {
                Sleep(1);
            }

            if(!all_shared->msock->is_open())
                break;
        }
    }
    catch(...)
    {

    }

    all_shared->shared.should_terminate = true;

    all_shared->shared.termination_count++;

    std::cout << "shutdown write" << std::endl;
}

void thread_session(
    tcp::socket&& socket,
    int64_t my_id,
    connection_t conn_type)
{
    lg::log("preconstr\n");

    std::shared_ptr<shared_command_handler_state> all_shared = std::make_shared<shared_command_handler_state>(std::move(socket), conn_type);

    std::deque<std::string> shared_queue;
    std::mutex shared_lock;

    lg::log("thread_session\n");

    //global_shared_data* store = fetch_global_shared_data();
    //store->add(&all_shared->shared);

    std::thread(read_queue, all_shared, std::ref(shared_queue), std::ref(shared_lock)).detach();
    std::thread(write_queue, all_shared).detach();

    ///3rd thread is the js exec context
    while(all_shared->shared.termination_count != 3 || all_shared->state.number_of_realtime_scripts_terminated != all_shared->state.number_of_realtime_scripts)
    {
        if(all_shared->state.number_of_realtime_scripts_terminated == all_shared->state.number_of_realtime_scripts)
        {
            all_shared->state.should_terminate_any_realtime = false;
        }

        Sleep(500);
    }

    /*{
        safe_lock_guard guard(store->lock);

        for(int i=0; i < (int)store->data.size(); i++)
        {
            if(store->data[i] == &all_shared->shared)
            {
                store->data.erase(store->data.begin() + i);
                break;
            }
        }
    }*/

    Sleep(50);

    std::cout << "shutdown session" << std::endl;
}

void session_wrapper(tcp::socket&& socket,
                     std::string const& doc_root,
                     int64_t my_id,
                     connection_t conn_type)
{
    try
    {
        thread_session(std::move(socket), my_id, conn_type);
    }
    catch(const std::exception& e)
    {
        std::cout << "e " << e.what() << std::endl;
    }
    catch(...)
    {
        std::cout << "threw error\n";
    }
}

/*void http_test_server()
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
                id,
                connection_type::HTTP).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "HTTP Error: " << e.what() << std::endl;
        //return EXIT_FAILURE;
    }
}*/

global_state glob;

void websocket_test_server()
{
    try
    {
        auto const address = boost::asio::ip::make_address(HOST_IP);
        auto const port = static_cast<unsigned short>(HOST_WEBSOCKET_PORT);
        std::string const doc_root = "./doc_root";

        // The io_context is required for all I/O
        boost::asio::io_context ioc{1};

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

void websocket_ssl_test_server(int in_port)
{
    try
    {
        auto const address = boost::asio::ip::make_address(HOST_IP);
        auto const port = static_cast<unsigned short>(in_port);
        std::string const doc_root = "./doc_root";

        // The io_context is required for all I/O
        boost::asio::io_context ioc{1};

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};
        for(;;)
        {
            lg::log("presock");

            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            lg::log("postaccept\n");

            int id = glob.global_id++;

            // Launch the session, transferring ownership of the socket
            std::thread(
                session_wrapper,
                std::move(socket),
                doc_root,
                id,
                connection_type::WEBSOCKET_SSL).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Websocket Error: " << e.what() << std::endl;
        //return EXIT_FAILURE;
    }
}

void boot_connection_handlers()
{
    //std::thread{std::bind(&http_test_server, &req)}.detach();

    start_non_user_task_thread();

    //std::thread(http_test_server).detach();
    //std::thread(websocket_test_server).detach();

    std::thread(websocket_test_server).detach();
    std::thread(websocket_ssl_test_server, HOST_WEBSOCKET_SSL_PORT).detach();
    std::thread(websocket_ssl_test_server, HOST_WEBSOCKET_SSL_PORT_2).detach();

    //http_test_server();
}
