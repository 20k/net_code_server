#ifndef SOCKET_SHARED_HPP_INCLUDED
#define SOCKET_SHARED_HPP_INCLUDED

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>

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

struct socket_interface
{
    virtual bool write(const std::string& msg) {return false;};
    virtual bool read(boost::system::error_code& ec){return false;};

    virtual std::string get_read(){return std::string();};

    virtual void shutdown(){};

    virtual bool is_open(){return false;};

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

    virtual bool write(const std::string& msg) override
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

        return false;
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

    websock_socket(tcp::socket&& sock, bool is_client) : ws(std::move(sock)){}

    template<typename T>
    void client_connect(const std::string& host, const T& results)
    {
        boost::asio::connect(ws.next_layer(), results.begin(), results.end());
        ws.handshake(host, "/");
    }

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
        std::ostringstream os;
        os << boost::beast::buffers(mbuffer.data());

        return os.str();
    }

    virtual bool write(const std::string& msg) override
    {
        ws.text(true);

        ws.write(boost::asio::buffer(msg), lec);

        if(lec)
        {
            fail(lec, "write");
            return true;
        }

        return false;
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


#endif // SOCKET_SHARED_HPP_INCLUDED
