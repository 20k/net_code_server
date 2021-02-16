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
#include <set>
#include <toolkit/clock.hpp>

#include "command_handler.hpp"

struct file_cache
{
    std::map<std::string, std::string> cache;

    std::string& get(const std::string& name)
    {
        if(auto it = cache.find(name); it != cache.end())
            return it->second;

        std::string data;

        if(file_exists(name))
            data = read_file_bin(name);

        cache[name] = data;

        return cache[name];
    }
};

void websocket_server(connection& conn)
{
    std::map<int, std::shared_ptr<shared_command_handler_state>> user_states;
    std::map<int, std::deque<nlohmann::json>> command_queue;
    std::map<int, steady_timer> time_since_join;

    std::set<int> http_clients;

    steady_timer ping_timer;
    steady_timer poll_clock; ///!!!
    steady_timer disconnect_clock;

    connection_received_data received_data;
    connection_send_data send_data(conn.get_settings());

    file_cache fcache;

    while(1)
    {
        try
        {
            conn.receive_bulk(received_data);

            for(auto& i : received_data.new_http_clients)
            {
                http_clients.insert(i);
            }

            for(auto& i : received_data.upgraded_to_websocket)
            {
                auto it = http_clients.find(i);

                if(it != http_clients.end())
                    http_clients.erase(it);
            }

            for(auto& i : received_data.http_read_queue)
            {
                for(auto& req : i.second)
                {
                    http_write_info dat;
                    dat.id = i.first;
                    dat.code = http_write_info::status_code::ok;
                    dat.mime_type = "text/plain";

                    if(req.path.ends_with("index.html"))
                    {
                        dat.mime_type = "text/html";
                        dat.body = fcache.get("./doc_root/index.html");
                    }

                    else if(req.path.ends_with("index.js"))
                    {
                        dat.mime_type = "text/javascript";
                        dat.body = fcache.get("./doc_root/index.js");
                    }

                    else if(req.path.ends_with("index.worker.js"))
                    {
                        dat.mime_type = "text/javascript";
                        dat.body = fcache.get("./doc_root/index.worker.js");
                    }

                    else if(req.path.ends_with("index.wasm"))
                    {
                        dat.mime_type = "application/wasm";
                        dat.body = fcache.get("./doc_root/index.wasm");
                    }
                    else
                    {
                        dat.mime_type = "text/html";
                        dat.body = fcache.get("./doc_root/index.html");
                    }

                    dat.keep_alive = req.keep_alive;

                    if(!send_data.write_to_http_unchecked(dat))
                    {
                        printf("Exception in http write\n");
                        send_data.disconnect(i.first);
                    }
                }
            }

            for(uint64_t next_client : received_data.new_clients)
            {
                user_states[next_client] = std::make_shared<shared_command_handler_state>();
                time_since_join[next_client].restart();
            }

        {
            for(uint64_t disconnected_client : received_data.disconnected_clients)
            {
                //printf("Disconnected Client\n");

                int disconnected_id = disconnected_client;

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

                if(http_clients.find(disconnected_id) != http_clients.end())
                {
                    http_clients.erase(disconnected_id);
                }
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

                    if(!send_data.write_to_websocket(wdat))
                    {
                        std::cout << "Exceeded write capacity in server write" << std::endl;
                        send_data.disconnect(i.first);
                    }
                }
                catch(std::exception& err)
                {
                    std::cout << "Exception in server ping " << err.what() << std::endl;
                    send_data.disconnect(i.first);
                }
            }

            ping_timer.restart();
        }

        //while(auto next_read = received_data.get_next_read())
        for(auto [id, all_data] : received_data.websocket_read_queue)
        {
            if(user_states.find(id) == user_states.end())
                continue;

            for(const write_data& dat : all_data)
            {
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
                    send_data.disconnect(dat.id);
                    break;
                }
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
                    to_write.data = std::move(next_command);

                    if(!send_data.write_to_websocket(to_write))
                    {
                        std::cout << "Exceeded write capacity in server write" << std::endl;
                        send_data.disconnect(i.first);
                        break;
                    }
                }
                catch(std::exception& e)
                {
                    std::cout << "Exception in server write " << e.what() << std::endl;
                    send_data.disconnect(i.first);
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

        conn.send_bulk(send_data);

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
