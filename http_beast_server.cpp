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
#include <toolkit/clock.hpp>

#include "command_handler.hpp"

void websocket_server(connection& conn)
{
    std::map<int, std::shared_ptr<shared_command_handler_state>> user_states;
    std::map<int, std::deque<nlohmann::json>> command_queue;
    std::map<int, steady_timer> time_since_join;

    steady_timer ping_timer;
    steady_timer poll_clock; ///!!!
    steady_timer disconnect_clock;

    while(1)
    {
        try
        {
        {
            std::optional<uint64_t> next_client = conn.has_new_client();

            while(next_client.has_value())
            {
                user_states[next_client.value()] = std::make_shared<shared_command_handler_state>();
                time_since_join[next_client.value()].restart();

                conn.pop_new_client();

                next_client = conn.has_new_client();

                //printf("New client\n");
            }
        }

        {
            std::optional<uint64_t> disconnected_client = conn.has_disconnected_client();

            while(disconnected_client.has_value())
            {
                printf("Disconnected Client\n");

                conn.pop_disconnected_client();

                int disconnected_id = disconnected_client.value();

                if(user_states.find(disconnected_id) != user_states.end())
                {
                    user_states[disconnected_id]->state.should_terminate_any_realtime = true;

                    user_states.erase(disconnected_id);
                }

                if(command_queue.find(disconnected_id) != command_queue.end())
                {
                    command_queue.erase(disconnected_id);
                }

                if(time_since_join.find(disconnected_id) != time_since_join.end())
                {
                    time_since_join.erase(disconnected_id);
                }

                disconnected_client = conn.has_disconnected_client();
            }
        }

        if(ping_timer.get_elapsed_time_s() > 2)
        {
            for(auto& i : user_states)
            {
                if(!i.second->state.is_authenticated())
                    continue;

                try
                {
                    nlohmann::json data;
                    data["type"] = "server_ping";

                    write_data wdat;
                    wdat.id = i.first;
                    wdat.data = data.dump();

                    conn.write_to(wdat);
                }
                catch(std::exception& err)
                {
                    std::cout << "Exception in server ping " << err.what() << std::endl;
                    conn.force_disconnect(i.first);
                }
            }

            ping_timer.restart();
        }

        while(conn.has_read())
        {
            write_data dat = conn.read_from();

            conn.pop_read(dat.id);

            if(user_states.find(dat.id) == user_states.end())
                continue;

            try
            {
                nlohmann::json parsed;
                parsed = nlohmann::json::parse(dat.data);

                //printf("Reading from %" PRIu64 "\n", dat.id);

                command_queue[dat.id].push_back(std::move(parsed));
            }
            catch(std::exception& err)
            {
                std::cout << "Caught json parse exception " << err.what() << std::endl;
                conn.force_disconnect(dat.id);
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

                try
                {
                    write_data to_write;
                    to_write.id = i.first;
                    to_write.data = next_command;

                    conn.write_to(to_write);
                }
                catch(std::exception& e)
                {
                    std::cout << "Exception in server write " << e.what() << std::endl;
                    conn.force_disconnect(i.first);
                    break;
                }

                write_count++;
            }
        }

        /*if(disconnect_clock.get_elapsed_time_s() > 5)
        {
            ///disconnect unauthed users
            for(auto& i : user_states)
            {
                int auth_time_ms = 8000;

                std::shared_ptr<shared_command_handler_state>& shared_state = i.second;

                if(!shared_state->state.is_authenticated() && (time_since_join[i.first].get_elapsed_time_s() * 1000) > auth_time_ms)
                {
                    conn.force_disconnect(i.first);
                }
            }

            disconnect_clock.restart();
        }*/

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

        for(auto& i : user_states)
        {
            if((i.second->terminate_timer.get_elapsed_time_s() * 1000) > 100)
            {
                i.second->state.should_terminate_any_realtime = false;
            }
        }

        if((poll_clock.get_elapsed_time_s() * 1000) > 500)
        {
            poll_clock.restart();

            for(auto& i : user_states)
            {
                nlohmann::json fake;
                fake["type"] = "client_poll";

                async_handle_command(i.second, std::move(fake));
            }
        }
        }
        catch(std::exception& e)
        {
            std::cout << "Critical Server Screwup " << e.what() << std::endl;
        }

        sf::sleep(sf::milliseconds(1));
    }
}

void boot_connection_handlers()
{
    start_non_user_task_thread();

    /*connection* c1 = new connection;
    c1->host("0.0.0.0", HOST_WEBSOCKET_SSL_PORT, connection_type::SSL);*/

    connection_settings sett;
    sett.max_window_bits = 15; ///9=0.9mb/s traffic, 15 = 0.2mb/s traffic
    sett.max_write_size = MAX_MESSAGE_SIZE;
    sett.max_read_size = MAX_MESSAGE_SIZE;

    connection* c2 = new connection;
    c2->host("0.0.0.0", HOST_WEBSOCKET_SSL_PORT_2, connection_type::SSL, sett);

    //connection* c3 = new connection;
    //c3->host("0.0.0.0", HOST_WEBSOCKET_PORT, connection_type::PLAIN);

    //sthread(websocket_server, std::ref(*c1)).detach();
    sthread(websocket_server, std::ref(*c2)).detach();

    connection* c3 = new connection;
    c3->host("0.0.0.0", HOST_WEBSOCKET_PORT, connection_type::PLAIN, sett);

    sthread(websocket_server, std::ref(*c3)).detach();

    //sthread(websocket_server, std::ref(*c3)).detach();
}
