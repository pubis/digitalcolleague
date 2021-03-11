#pragma once

#include "common.hpp"

namespace dc {

namespace console {

struct settings {
  bool enabled;
  int port;
};

settings tag_invoke(json::value_to_tag<settings>, const json::value& jv);

using boost::asio::ip::tcp;

class connection : public std::enable_shared_from_this<connection> {
  tcp::socket socket_;
  asio::streambuf buffer_;
  std::deque<std::string> write_queue;

  void send(std::string data);
  void send_line(std::string data);
  void on_command(const std::string& command);

  void do_write();

  void await_command();

public:
  using pointer = std::shared_ptr<connection>;

  connection(tcp::socket socket);

  void start();
};

class server {
  asio::io_context& ctx;
  settings settings_;
  tcp::acceptor acceptor;

public:
  server(asio::io_context& ctx, const settings& settings);

private:
  void start_accept();

  void handle_accept(connection::pointer connection, const boost::system::error_code& error);
};

} // console

} // dc
