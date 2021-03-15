#pragma once

#include "common.hpp"

namespace dc {

namespace console {

struct settings {
  bool enabled;
  int port;
};

settings tag_invoke(json::value_to_tag<settings>, const json::value& jv);

using asio::ip::tcp;

class server;

class connection : public std::enable_shared_from_this<connection> {
  tcp::socket socket_;
  asio::streambuf buffer_;
  std::deque<std::string> write_queue;
  server* server_;

  void send(std::string data);
  void send_line(std::string data);
  void on_command(const std::string& command);

  void do_write();

  void await_command();

public:
  using pointer = std::shared_ptr<connection>;

  connection(tcp::socket socket, server* server);

  void start();
};

class server {
  using command_handler = std::function<void(std::string_view)>;

  asio::io_context& ctx;
  settings settings_;
  tcp::acceptor acceptor;
  std::unordered_map<std::string, std::vector<command_handler>> command_handlers_;

public:
  server(asio::io_context& ctx, const settings& settings);

  void register_handler(std::string name, command_handler handler);
  void handle_command(const std::string& command);

private:
  void start_accept();

  void handle_accept(connection::pointer connection, const std::error_code& error);
};

} // console

} // dc
