#include <deque>
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
  asio::streambuf buffer_;
  std::deque<std::string> write_queue;

  void send(std::string data);
  void send_line(std::string data);
  void on_command(const std::string& command);

  void do_write();

  void await_command();

public:
  using pointer = std::shared_ptr<tcp_connection>;

  tcp_connection(tcp::socket socket);

  void start();
};

class tcp_server {
  asio::io_context& ctx;
  tcp::acceptor acceptor;

public:
  tcp_server(asio::io_context& ctx);

private:
  void start_accept();

  void handle_accept(tcp_connection::pointer connection, const boost::system::error_code& error);
};

}
