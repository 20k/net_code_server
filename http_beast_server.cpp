#include "http_beast_server.hpp"
#include "non_user_task_thread.hpp"
#include "../crapmud_client/http_beast_client.hpp"

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

//------------------------------------------------------------------------------

// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view
mime_type(boost::beast::string_view path)
{
    using boost::beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == boost::beast::string_view::npos)
            return boost::beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    boost::beast::string_view base,
    boost::beast::string_view path)
{
    if(base.empty())
        return path.to_string();
    std::string result = base.to_string();
#if BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    boost::beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send,
    const std::string& sbody)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](boost::beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](boost::beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](boost::beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if( req.method() != http::verb::get &&
        req.method() != http::verb::head)
        return send(bad_request("Unknown HTTP-method"));

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != boost::beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if(req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    boost::beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), boost::beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if(ec == boost::system::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    // Handle an unknown error
    if(ec)
        return send(server_error(ec.message()));

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if(req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    http::response<http::string_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(sbody)),
        std::make_tuple(http::status::ok, req.version())};

    //http::response<http::string_body> res;

    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    //res.set(http::field::content_type, mime_type(path));
    //res.content_length(size);
    res.keep_alive(req.keep_alive());

    res.set(http::field::content_type, "text/plain");
    //res.body() = sbody;
    res.prepare_payload();

    return send(std::move(res));
}

//------------------------------------------------------------------------------

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

// Handles an HTTP server connection
void read_queue(tcp::socket& socket,
                std::string const& doc_root,
                global_state& glob,
                int64_t my_id,
                shared_data& shared)
{
    command_handler_state state;

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

                printf("got test read\n");

                ///got a request
                std::string to_pipe = handle_command(state, req.body(), glob, my_id);

                shared.add_back_write(to_pipe);
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

            Sleep(50);

            if(shared.has_front_write())
            {
                printf("sending test write\n");

                std::string next_command = shared.get_front_write();

                if(next_command == "")
                {
                    printf("skipping\n");
                    continue;
                }

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

    global_shared_data* store = fetch_global_shared_data();
    store->add(&shared);

    std::thread(read_queue, std::ref(socket), doc_root, std::ref(glob), my_id, std::ref(shared)).detach();
    std::thread(write_queue, std::ref(socket), doc_root, std::ref(glob), my_id, std::ref(shared)).detach();

    while(shared.termination_count != 2)
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

///Ok so: This session is a proper hackmud worker thread thing
///we should wait for requests
///need to ensure we never end up with two of the same user
void
do_session(
    tcp::socket&& socket,
    std::string const& doc_root,
    global_state& glob,
    command_handler_state& state,
    int64_t my_id)
{
    bool close = false;
    boost::system::error_code ec;

    // This buffer is required to persist across reads
    boost::beast::flat_buffer buffer;

    // This lambda is used to send messages
    send_lambda<tcp::socket> lambda{socket, close, ec};

    for(;;)
    {
        // Read a request
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);

        //std::cout << "rq " << req.body() << std::endl;

        if(ec == http::error::end_of_stream)
            break;
        if(ec)
            return fail(ec, "read");

        std::string to_pipe = handle_command(state, req.body(), glob, my_id);

        // Send the response
        handle_request(doc_root, std::move(req), lambda, to_pipe);
        if(ec)
            return fail(ec, "write");
        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            break;
        }
    }

    // Send a TCP shutdown
    socket.shutdown(tcp::socket::shutdown_send, ec);

    std::cout << "shutdown\n" << std::endl;

    // At this point the connection is closed gracefully
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

    /*std::lock_guard<std::mutex> lk(glob.auth_lock);

    ///oh crap
    ///we actually have to use the db for auth locks long term
    ///otherwise you could connect to a different server
    glob.auth_locks[state.auth] = 0;*/
}

//------------------------------------------------------------------------------

#if 0
int main(int argc, char* argv[])
{
    try
    {
        // Check command line arguments.
        if (argc != 4)
        {
            std::cerr <<
                "Usage: http-server-sync <address> <port> <doc_root>\n" <<
                "Example:\n" <<
                "    http-server-sync 0.0.0.0 8080 .\n";
            return EXIT_FAILURE;
        }
        auto const address = boost::asio::ip::make_address(argv[1]);
        auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
        std::string const doc_root = argv[3];

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

            // Launch the session, transferring ownership of the socket
            std::thread{std::bind(
                &do_session,
                std::move(socket),
                doc_root)}.detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
#endif
#if 0
void http_test_run_async()
{
    const auto address = boost::asio::ip::make_address("127.0.0.1");
    const auto port = static_cast<unsigned short>(6750);
    const std::string doc_root = "./doc_root";
    auto const threads = 2;

    boost::asio::io_context ioc{threads};

    std::make_shared<listener>(
        ioc,
        tcp::endpoint{address, port},
        doc_root)->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });


    ioc.run();
}
#endif // 0

void http_test_server()
{
    try
    {
        /*// Check command line arguments.
        if (argc != 4)
        {
            std::cerr <<
                "Usage: http-server-sync <address> <port> <doc_root>\n" <<
                "Example:\n" <<
                "    http-server-sync 0.0.0.0 8080 .\n";
            return EXIT_FAILURE;
        }*/
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
