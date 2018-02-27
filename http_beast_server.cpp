#include "http_beast_server.hpp"
#include "non_user_task_thread.hpp"
#include "shared_data.hpp"

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


#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "command_handler.hpp"

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
namespace websocket = boost::beast::websocket;

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template<class Stream>
struct send_lambda
{
    Stream& stream_;
    bool& close_;
    boost::system::error_code& ec_;

    explicit
    send_lambda(
        Stream& stream,
        bool& close,
        boost::system::error_code& ec)
        : stream_(stream)
        , close_(close)
        , ec_(ec)
    {
    }

    template<bool isRequest, class Body, class Fields>
    void
    operator()(http::message<isRequest, Body, Fields>&& msg) const
    {
        // Determine if we should close the connection after
        close_ = msg.need_eof();

        // We need the serializer here because the serializer requires
        // a non-const file_body, and the message oriented version of
        // http::write only works with const messages.
        http::serializer<isRequest, Body, Fields> sr{msg};
        http::write(stream_, sr, ec_);
    }
};

namespace connection_type
{
    enum connection_type
    {
        WEBSOCKET,
        HTTP
    };
}

using connection_t = connection_type::connection_type;

struct socket_interface
{
    virtual void write(const std::string& msg);
    virtual bool read(boost::system::error_code& ec);

    virtual std::string get_read();

    virtual void shutdown();

    virtual bool is_open();

    virtual ~socket_interface(){}
};

struct http_socket : socket_interface
{
    tcp::socket socket;

    boost::beast::flat_buffer buffer;
    http::request<http::string_body> req;

    send_lambda<tcp::socket> lambda;//{socket, close, ec};

    boost::system::error_code lec;
    bool close = false;

    http_socket(tcp::socket&& sock) : socket(std::move(sock)), lambda{socket, close, lec} {}

    virtual bool read(boost::system::error_code& ec) override
    {
        req = http::request<http::string_body>();
        buffer = boost::beast::flat_buffer();

        http::read(socket, buffer, req, ec);

        if(ec == http::error::end_of_stream)
            return true;
        if(ec)
        {
            fail(ec, "read");
            return true;
        }

        return false;
    }

    virtual std::string get_read() override
    {
        return req.body();
    }

    virtual void write(const std::string& msg) override
    {
        http::response<http::string_body> res{
                    std::piecewise_construct,
                    std::make_tuple(std::move(msg)),
                    std::make_tuple(http::status::ok, 11)};

        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.keep_alive(true);

        res.set(http::field::content_type, "text/plain");
        res.prepare_payload();

        lambda(std::move(res));
    }

    virtual void shutdown() override
    {
        socket.shutdown(tcp::socket::shutdown_send, lec);
    }

    virtual bool is_open() override
    {
        return socket.is_open();
    }
};

struct websock_socket : socket_interface
{
    boost::beast::websocket::stream<tcp::socket> ws;
    boost::beast::multi_buffer mbuffer;
    boost::system::error_code lec;

    websock_socket(tcp::socket&& sock) : ws(std::move(sock)) {ws.accept();}

    virtual bool read(boost::system::error_code& ec) override
    {
        mbuffer = decltype(mbuffer)();

        ws.read(mbuffer, ec);

        if(ec)
        {
            fail(ec, "read");
            return true;
        }

        return false;
    }

    std::string get_read() override
    {
        auto bufs = mbuffer.data();
        std::string s(boost::beast::buffers_to_string(bufs), mbuffer.size());

        return s;
    }

    virtual void write(const std::string& msg) override
    {
        ws.text(true);

        ws.write(boost::asio::buffer(msg));
    }

    virtual void shutdown() override
    {
        ws.close(boost::beast::websocket::close_code::normal, lec);
    }

    virtual bool is_open() override
    {
        return ws.is_open();
    }
};

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

        std::string to_pipe = handle_command(state, to_exec, glob, my_id);
        shared.add_back_write(to_pipe);

        Sleep(5);
    }

    std::cout << "shutdown async" << std::endl;

    shared.termination_count++;
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

            Sleep(50);

            //if(socket.available() > 0)
            {
                if(socket.read(ec))
                    break;

                std::string next_command = socket.get_read();

                int len;

                bool rate_hit = true;

                {
                    std::lock_guard lg(shared_lock);

                    len = shared_queue.size();

                    std::string str = next_command;

                    if(len <= 10 || starts_with(str, "client_poll"))
                    {
                        shared_queue.push_back(next_command);
                        rate_hit = false;
                    }
                }

                if(rate_hit)
                    shared.add_back_write("command Hit rate limit (read_queue)");
            }
        }
    }
    catch(...)
    {

    }

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

                if(next_command != "")
                    printf("sending test write\n");

                socket.write(next_command);
            }
            else
            {
                Sleep(50);
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
    while(shared.termination_count != 3)
    {
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

    std::thread(http_test_server).detach();
    //std::thread(websocket_test_server).detach();

    websocket_test_server();

    //start_non_user_task_thread();
    //http_test_server();
}
