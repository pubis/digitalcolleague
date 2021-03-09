#include "console.hpp"

namespace dc {

settings tag_invoke(json::value_to_tag<settings>, const json::value& jv) {
  settings s;
  const json::object& obj = jv.as_object();
  extract(obj, s.enabled, "enabled");
  extract(obj, s.port, "port");
  return s;
}

using boost::asio::ip::tcp;

tcp_connection::tcp_connection(tcp::socket socket)
  : socket_(std::move(socket))
{}

void tcp_connection::send(std::string data) {
  bool write_in_progress = !write_queue.empty();
  write_queue.push_back(std::move(data));

  if (!write_in_progress)
    do_write();
}

void tcp_connection::send_line(std::string data) {
  data += "\n";
  send(std::move(data));
}

void tcp_connection::do_write() {
  auto self(shared_from_this());
  asio::async_write(socket_,
      asio::buffer(write_queue.front().data(), write_queue.front().size()),
      [this, self](const boost::system::error_code& error, std::size_t /* length */) {
        if (!error) {
          write_queue.pop_front();
          if (!write_queue.empty()) {
            do_write();
          }
        } else {
          std::cerr << "[Console] Write error: " << error.message() << '\n';
        }
      }
  );

}

void tcp_connection::on_command(const std::string& command) {
  std::cout << "[Console] Command: " << command << '\n';

  send(": ");
}

void tcp_connection::await_command() {
  auto self(shared_from_this());

  auto handler = [this, self](const auto& error, std::size_t s) {
    if (error) {
      std::cerr << "[Console] Disconnected\n";
      return;
    }

    std::istream istrm{ &buffer_ };
    std::string line;
    std::getline(istrm, line);

    on_command(line);

    await_command();
  };

  asio::async_read_until(socket_, buffer_, "\n", handler);
}

void tcp_connection::start() {
  std::cout << "[Console] Connection from " << socket_.remote_endpoint() << " accepted\n";

  send_line("HENLO");
  send(": ");

  await_command();
}

tcp_server::tcp_server(asio::io_context& ctx, const settings& settings)
  : ctx(ctx)
  , settings_(settings)
  , acceptor(ctx, tcp::endpoint(tcp::v4(), settings_.port))
{
  if (settings_.enabled)
    start_accept();
}

void tcp_server::start_accept() {
  acceptor.async_accept(
      [this](const boost::system::error_code& error, tcp::socket socket) {
        if (!error) {
          std::make_shared<tcp_connection>(std::move(socket))->start();
        }

        start_accept();
      }
  );

  std::cout << "[Console] Accepting connections on port " << settings_.port << '\n';
}

void tcp_server::handle_accept(tcp_connection::pointer connection, const boost::system::error_code& error) {
  if (!error) {
    connection->start();
  } else {
    std::cerr << "[Console] Accept error: " << error.message() << '\n';
  }

  start_accept();
}

}
