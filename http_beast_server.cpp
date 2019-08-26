#include <libncclient/socket.hpp>

#include "http_beast_server.hpp"
#include "non_user_task_thread.hpp"
#include "shared_data.hpp"
#include "logging.hpp"
#include <libncclient/nc_util.hpp>
#include <nlohmann/json.hpp>
#include "shared_command_handler_state.hpp"
#include "safe_thread.hpp"
#include <networking/networking.hpp>

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

#include <boost/asio.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "command_handler.hpp"

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

bool handle_termination_shortcircuit(const std::shared_ptr<shared_command_handler_state>& all_shared, const std::string& str, sf::Clock& terminate_timer)
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

                terminate_timer.restart();
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
                        {
                            unprocessed_key_info info;
                            info.key = i;
                            info.is_repeat = all_shared->state.get_key_state(id)[i];

                            all_shared->state.unprocessed_keystrokes[id].push_back(info);
                        }

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

    /*if(str == "client_poll" || str == "client_poll_json")
    {
        std::string out = handle_command(all_shared, str);

        all_shared->shared.add_back_write(out);

        return true;
    }*/

    return false;
}

#if 0
bool handle_read(const std::shared_ptr<shared_command_handler_state>& all_shared, std::deque<std::string>& shared_queue, sf::Clock& terminate_timer)
{
    boost::system::error_code ec;

    if(all_shared->msock->read(ec))
        return true;

    std::string next_command = all_shared->msock->get_read();

    if(next_command.size() > 400000)
        return false;

    if(handle_termination_shortcircuit(all_shared, next_command, terminate_timer))
        return false;

    lg::log(next_command);

    int len;

    bool rate_hit = true;

    {
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

    return false;
}

bool handle_write(const std::shared_ptr<shared_command_handler_state>& all_shared)
{
    std::string next_command = all_shared->shared.get_front_write();

    if(next_command == "" && all_shared->type != connection_type::HTTP)
        return false;

    if(next_command.size() > MAX_MESSAGE_SIZE)
    {
        next_command.resize(MAX_MESSAGE_SIZE);

        next_command += " [Truncated, > " + std::to_string(MAX_MESSAGE_SIZE) + "]";
    }

    if(all_shared->msock->write(next_command))
        return true;

    return false;
}

void read_write_queue(std::shared_ptr<shared_command_handler_state> all_shared)
{
    std::deque<std::string> shared_queue;

    sf::Clock ping_clock;
    ///time after which i should ping
    double ping_time_ms = 2000;

    lg::log("read_write_async_queue\n");

    sf::Clock terminate_timer;

    try
    {
        while(1)
        {
            if(all_shared->msock->timed_out())
                break;

            if(!all_shared->msock->is_open())
                break;

            bool found_any = false;

            int read_count = 0;

            while(all_shared->msock->available() > 0 && read_count < 100)
            {
                found_any = true;

                if(handle_read(all_shared, shared_queue, terminate_timer))
                    break;

                read_count++;
            }

            if(terminate_timer.getElapsedTime().asMilliseconds() > 100)
            {
                all_shared->state.should_terminate_any_realtime = false;
            }

            int write_count = 0;

            while(all_shared->shared.has_front_write() && write_count < 100)
            {
                found_any = true;

                ping_clock.restart();

                if(handle_write(all_shared))
                    break;

                write_count++;
            }

            if(shared_queue.size() > 0)
            {
                if(!all_shared->execution_is_blocked && !all_shared->execution_requested)
                {
                    found_any = true;

                    all_shared->execution_requested = true;

                    async_handle_command(all_shared, shared_queue.front());

                    //conditional_async_handle_command(all_shared, shared_queue.front());

                    shared_queue.pop_front();
                }
            }

            if(ping_clock.getElapsedTime().asMilliseconds() > ping_time_ms)
            {
                ping_clock.restart();

                if(all_shared->msock->write("command_ping"))
                    break;
            }

            if(!found_any)
            {
                sthread::this_sleep(2);
            }
        }
    }
    catch(...)
    {

    }

    all_shared->state.should_terminate_any_realtime = true;

    all_shared->msock->shutdown();

    std::cout << "shutdown read/write/async" << std::endl;
}

void thread_session(
    tcp::socket&& socket,
    connection_t conn_type)
{
    lg::log("preconstr\n");

    std::shared_ptr<shared_command_handler_state> all_shared = std::make_shared<shared_command_handler_state>(std::move(socket), conn_type);

    lg::log("thread_session\n");

    //global_shared_data* store = fetch_global_shared_data();
    //store->add(&all_shared->shared);

    read_write_queue(all_shared);

    while(all_shared->state.number_of_running_realtime_scripts() != 0)
    {
        if(all_shared->state.number_of_running_realtime_scripts() == 0)
        {
            all_shared->state.should_terminate_any_realtime = false;
            break;
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
                     connection_t conn_type)
{
    try
    {
        thread_session(std::move(socket), conn_type);
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

void websocket_ssl_test_server(int in_port)
{
    try
    {
        auto const address = boost::asio::ip::make_address(HOST_IP);
        auto const port = static_cast<unsigned short>(in_port);

        // The io_context is required for all I/O
        boost::asio::io_context ioc{2};

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};
        for(;;)
        {
            lg::log("presock");

            std::cout << "presock\n";

            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            std::cout << "accepted\n";

            lg::log("postaccept\n");

            // Launch the session, transferring ownership of the socket
            sthread(
                session_wrapper,
                std::move(socket),
                connection_type::WEBSOCKET_SSL).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Websocket Error: " << e.what() << std::endl;
        //return EXIT_FAILURE;
    }
}
#endif // 0

void websocket_ssl_reformed(int in_port)
{
    connection conn;
    conn.host("0.0.0.0", in_port);

    std::map<int, std::shared_ptr<shared_command_handler_state>> user_states;
    std::map<int, std::deque<std::string>> command_queue;
    std::map<int, sf::Clock> terminate_timers;

    sf::Clock ping_timer;
    sf::Clock poll_clock; ///!!!

    while(1)
    {
        {
            std::optional<uint64_t> next_client = conn.has_new_client();

            while(next_client.has_value())
            {
                user_states[next_client.value()] = std::make_shared<shared_command_handler_state>();

                conn.pop_new_client();

                next_client = conn.has_new_client();
            }
        }

        {
            std::optional<uint64_t> disconnected_client = conn.has_disconnected_client();

            while(disconnected_client.has_value())
            {
                conn.pop_disconnected_client();

                if(user_states.find(disconnected_client.value()) != user_states.end())
                {
                    user_states[disconnected_client.value()]->state.should_terminate_any_realtime = true;

                    user_states.erase(disconnected_client.value());
                }

                disconnected_client = conn.has_disconnected_client();
            }
        }

        if(ping_timer.getElapsedTime().asSeconds() > 2)
        {
            for(auto& i : user_states)
            {
                write_data wdat;
                wdat.id = i.first;
                wdat.data = "command_ping";

                conn.write_to(wdat);
            }

            ping_timer.restart();
        }

        while(conn.has_read())
        {
            write_data dat = conn.read_from();

            conn.pop_read(dat.id);

            if(user_states.find(dat.id) == user_states.end())
                continue;

            if(dat.data.size() > 400000)
                continue;

            if(handle_termination_shortcircuit(user_states[dat.id], dat.data, terminate_timers[dat.id]))
                continue;

            command_queue[dat.id].push_back(dat.data);
        }

        for(auto& i : user_states)
        {
            std::shared_ptr<shared_command_handler_state>& shared_state = i.second;

            int write_count = 0;

            while(shared_state->shared.has_front_write() && write_count < 100)
            {
                std::string next_command = i.second->shared.get_front_write();

                if(next_command.size() == 0)
                    continue;

                if(next_command.size() > MAX_MESSAGE_SIZE)
                {
                    next_command.resize(MAX_MESSAGE_SIZE);

                    next_command += " [Truncated, > " + std::to_string(MAX_MESSAGE_SIZE) + "]";
                }

                write_data to_write;
                to_write.id = i.first;
                to_write.data = next_command;

                conn.write_to(to_write);

                write_count++;
            }
        }

        for(auto& i : command_queue)
        {
            if(user_states.find(i.first) == user_states.end())
                continue;

            std::shared_ptr<shared_command_handler_state>& shared = user_states[i.first];

            std::deque<std::string>& my_queue = i.second;

            if(my_queue.size() > 0)
            {
                if(!shared->execution_is_blocked && !shared->execution_requested)
                {
                    shared->execution_requested = true;

                    async_handle_command(shared, my_queue.front());

                    my_queue.pop_front();
                }
            }
        }

        for(auto& i : terminate_timers)
        {
            if(user_states.find(i.first) == user_states.end())
                continue;

            if(i.second.getElapsedTime().asMilliseconds() > 100)
            {
                user_states[i.first]->state.should_terminate_any_realtime = false;
            }
        }

        if(poll_clock.getElapsedTime().asMicroseconds() / 1000. > 500)
        {
            poll_clock.restart();

            for(auto& i : user_states)
            {
                std::string out = handle_command(i.second, "client_poll_json");

                write_data dat;
                dat.id = i.first;
                dat.data = out;

                conn.write_to(dat);
            }
        }

        sf::sleep(sf::milliseconds(100));
    }
}

void boot_connection_handlers()
{
    start_non_user_task_thread();

    sthread(websocket_ssl_reformed, HOST_WEBSOCKET_SSL_PORT).detach();
    sthread(websocket_ssl_reformed, HOST_WEBSOCKET_SSL_PORT_2).detach();

    //sthread(websocket_ssl_test_server, HOST_WEBSOCKET_SSL_PORT).detach();
    //sthread(websocket_ssl_test_server, HOST_WEBSOCKET_SSL_PORT_2).detach();
}
