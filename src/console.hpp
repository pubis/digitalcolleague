#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

namespace dc {

namespace asio = boost::asio;

using boost::asio::ip::tcp;

class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
  tcp::socket socket_;
  std::string message_;

  tcp_connection(asio::io_context& ctx)
    : socket_(ctx)
  {}

  void handle_write(const boost::system::error_code& error, size_t bytes) {
    if (!error)
      std::cout << "Sent " << bytes << " bytes\n";
    else
      std::cerr << "Write error: " << error.message() << '\n';
  }

public:
  using pointer = std::shared_ptr<tcp_connection>;

  static pointer create(asio::io_context& ctx) {
    return pointer(new tcp_connection(ctx));
  }

  tcp::socket& socket() {
    return socket_;
  }

  void start() {
    message_ = "HENLO";

    asio::async_write(socket_, asio::buffer(message_),
        boost::bind(&tcp_connection::handle_write, shared_from_this(),
          asio::placeholders::error,
          asio::placeholders::bytes_transferred));
  }
};

class tcp_server {
  asio::io_context& ctx;
  tcp::acceptor acceptor;

public:
  tcp_server(asio::io_context& ctx)
    : ctx(ctx)
    , acceptor(ctx, tcp::endpoint(tcp::v4(), 6969))
  {
    start_accept();
  }

private:
  void start_accept() {
    tcp_connection::pointer connection = tcp_connection::create(ctx);

    acceptor.async_accept(connection->socket(),
        boost::bind(&tcp_server::handle_accept, this, connection,
          asio::placeholders::error));

    std::cout << "Console accepting connections on port 6969\n";
  }

  void handle_accept(tcp_connection::pointer connection, const boost::system::error_code& error) {
    if (!error)
      connection->start();

    start_accept();
  }
};

}
