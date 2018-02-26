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
#endif // EXTERN_IP

#ifdef LOCAL_IP
#define HOST_PORT 6751
#endif // LOCAL_IP


#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "command_handler.hpp"

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

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

void async_command_handler(shared_data& shared, command_handler_state& state, std::deque<std::string>& shared_queue, std::mutex& shared_lock,
                           global_state& glob, int64_t my_id)
{
    while(1)
    {
        if(shared.should_terminate)
            break;

        std::string to_exec = "";

        {
            std::lock_guard lg(shared_lock);

            if(shared_queue.size() > 0)
            {
                to_exec = shared_queue.front();
                shared_queue.pop_front();
            }
        }

        if(to_exec == "")
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
void read_queue(tcp::socket& socket,
                std::string const& doc_root,
                global_state& glob,
                int64_t my_id,
                shared_data& shared,
                std::deque<std::string>& shared_queue,
                std::mutex& shared_lock,
                command_handler_state& state)
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
                boost::beast::flat_buffer buffer;
                http::request<http::string_body> req;
                http::read(socket, buffer, req, ec);

                if(ec == http::error::end_of_stream)
                    break;
                if(ec)
                {
                    fail(ec, "read");
                    break;
                }


                int len;

                bool rate_hit = true;

                {
                    std::lock_guard lg(shared_lock);

                    len = shared_queue.size();

                    std::string str = req.body();

                    if(len <= 5 || starts_with(str, "client_poll"))
                    {
                        shared_queue.push_back(req.body());
                        rate_hit = false;
                    }
                }

                if(rate_hit)
                    shared.add_back_write("command Hit rate limit (read_queue)");

                //printf("got test read\n");

                ///got a request
                //std::string to_pipe = handle_command(state, req.body(), glob, my_id);

                //shared.add_back_write(to_pipe);
            }
        }
    }
    catch(...)
    {

    }

    shared.should_terminate = true;

    socket.shutdown(tcp::socket::shutdown_send, ec);

    std::cout << "shutdown read" << std::endl;

    shared.termination_count++;
}

void write_queue(tcp::socket& socket,
                std::string const& doc_root,
                global_state& glob,
                int64_t my_id,
                shared_data& shared)
{
    try
    {
        bool close = false;
        boost::system::error_code ec;

        send_lambda<tcp::socket> lambda{socket, close, ec};

        while(1)
        {
            if(shared.should_terminate)
                break;

            if(shared.has_front_write())
            {
                std::string next_command = shared.get_front_write();

                if(next_command == "")
                    continue;

                printf("sending test write\n");

                /*http::request<http::string_body> req{http::verb::get, "./test.txt", 11};
                req.set(http::field::host, HOST_IP);
                req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

                req.set(http::field::content_type, "text/plain");
                req.body() = next_command;

                req.prepare_payload();

                http::write(*socket, req);*/

                http::response<http::string_body> res{
                    std::piecewise_construct,
                    std::make_tuple(std::move(next_command)),
                    std::make_tuple(http::status::ok, 11)};

                //http::response<http::string_body> res;

                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.keep_alive(true);

                res.set(http::field::content_type, "text/plain");
                res.prepare_payload();

                lambda(std::move(res));
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
    std::string const& doc_root,
    global_state& glob,
    int64_t my_id)
{
    shared_data shared;

    std::deque<std::string> shared_queue;
    std::mutex shared_lock;
    command_handler_state state;


    global_shared_data* store = fetch_global_shared_data();
    store->add(&shared);

    tcp::socket claimed_sock = std::move(socket);

    std::thread(read_queue, std::ref(claimed_sock), doc_root, std::ref(glob), my_id, std::ref(shared),
                std::ref(shared_queue), std::ref(shared_lock), std::ref(state)).detach();
    std::thread(write_queue, std::ref(claimed_sock), doc_root, std::ref(glob), my_id, std::ref(shared)).detach();

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

    std::cout << "shutdown session" << std::endl;
}

void session_wrapper(tcp::socket&& socket,
                     std::string const& doc_root,
                     global_state& glob,
                     int64_t my_id)
{
    try
    {
        thread_session(std::move(socket), doc_root, glob, my_id);
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
                id).detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        //return EXIT_FAILURE;
    }
}

void http_test_run()
{
    //std::thread{std::bind(&http_test_server, &req)}.detach();

    start_non_user_task_thread();
    http_test_server();
}
