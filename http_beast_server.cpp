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

bool handle_termination_shortcircuit(const std::shared_ptr<shared_command_handler_state>& all_shared, nlohmann::json data, sf::Clock& terminate_timer)
{
    if(data["type"] == "client_terminate_scripts")
    {
        int id = data["id"];

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

    if(data["type"] == "send_keystrokes_to_script")
    {
        int id = data["id"];

        ///todo
        ///keystroke special funtime
        ///create a push vector function
        ///need to also have an internal map of key state
        ///assume keys are not pressed when we get them the first time, dont do any magic

        if(data.find("input_keys") != data.end())
        {
            try
            {
                std::vector<std::string> str = data["input_keys"];
                str = sanitise_input_vec(str);

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

        if(data.find("pressed_keys") != data.end())
        {
            try
            {
                std::vector<std::string> str = data["pressed_keys"];
                str = sanitise_input_vec(str);

                for(auto& i : str)
                {
                    all_shared->state.set_key_state(id, i, true);
                }
            }
            catch(...){}
        }

        if(data.find("released_keys") != data.end())
        {
            try
            {
                std::vector<std::string> str = data["released_keys"];
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

    if(data["type"] == "send_script_info")
    {
        int id = data["id"];

        if(id < 0)
            return true;

        int width = data["width"];
        int height = data["height"];

        all_shared->state.set_width_height(id, width, height);

        return true;
    }

    std::string mstr = "client_script_mouseinput ";

    if(data["type"] == "update_mouse_to_script")
    {
        int id = data["id"];

        if(id < 0)
            return true;

        float mouse_x = data["mouse_x"];
        float mouse_y = data["mouse_y"];

        float mousewheel_x = data["mousewheel_x"];
        float mousewheel_y = data["mousewheel_y"];

        all_shared->state.add_mouse_state(id, {mouse_x, mouse_y}, {mousewheel_x, mousewheel_y});

        return true;
    }

    return false;
}

void websocket_ssl_reformed(int in_port)
{
    connection conn;
    conn.host("0.0.0.0", in_port, connection_type::SSL);

    std::map<int, std::shared_ptr<shared_command_handler_state>> user_states;
    std::map<int, std::deque<nlohmann::json>> command_queue;
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

                printf("New client\n");
            }
        }

        {
            std::optional<uint64_t> disconnected_client = conn.has_disconnected_client();

            while(disconnected_client.has_value())
            {
                printf("Disconnected Client\n");

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

            nlohmann::json parsed;

            try
            {
                parsed = nlohmann::json::parse(dat.data);

                if(handle_termination_shortcircuit(user_states[dat.id], parsed, terminate_timers[dat.id]))
                    continue;

                command_queue[dat.id].push_back(parsed);
            }
            catch(...)
            {
                continue;
            }
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

            std::deque<nlohmann::json>& my_queue = i.second;

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
