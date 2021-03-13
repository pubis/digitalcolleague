#include "session.hpp"

namespace dc {

namespace discord {

OpCode tag_invoke(json::value_to_tag<OpCode>, const json::value& jv) {
  return static_cast<OpCode>(json::value_to<int>(jv));
}

void Session::run(const Gateway& gateway, callback handler) {
  this->handler = handler;

  connect(gateway);
}

void Session::connect(const Gateway& gateway) {
  host = gateway.url.substr(gateway.url.find_last_of('/') + 1);
  std::cout << "[Discord] Connecting to: " << host << '\n';

  resolver.async_resolve(host, "443",
      beast::bind_front_handler(&Session::onResolve, shared_from_this()));
}

void Session::send(const json::object& data) {
  bool write_in_progress = !writeQueue.empty();
  writeQueue.push_back(json::serialize(data));

  if (!write_in_progress)
    doWrite();
}

void Session::disconnect() {
  std::cout << "[Discord] Disconnecting\n";

  ws.async_close(ws::close_code::normal,
      beast::bind_front_handler(&Session::onClose, shared_from_this()));
}


void Session::onResolve(beast::error_code ec, tcp::resolver::results_type results) {
  if (ec) {
    std::cerr << "[Discord] Resolve failed: " << ec.message() << '\n';
    return;
  }

  beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

  beast::get_lowest_layer(ws).async_connect(results,
      beast::bind_front_handler(&Session::onConnect, shared_from_this()));
}

void Session::onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
  if (ec) {
    std::cerr << "[Discord] Connect failed: " << ec.message() << '\n';
    return;
  }

  host += ':' + std::to_string(endpoint.port());

  beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

  if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
    ec = beast::error_code(static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category());
    std::cerr << "[Discord] SSL Error: " << ec.message() << '\n';
    return;
  }

  ws.next_layer().async_handshake(ssl::stream_base::client,
      beast::bind_front_handler(&Session::onSslHandshake, shared_from_this()));
}

void Session::onSslHandshake(beast::error_code ec) {
  if (ec) {
    std::cerr << "[Discord] SSL Handshake failed: " << ec.message() << '\n';
    return;
  }

  beast::get_lowest_layer(ws).expires_never();

  ws.set_option(ws::stream_base::timeout::suggested(beast::role_type::client));

  ws.set_option(ws::stream_base::decorator(
        [](ws::request_type& req) {
          req.set(http::field::user_agent, "DiscordBot (https://github.com/pubis, 1.0)");
        }
  ));

  ws.async_handshake(host, "/?v=6&encoding=json",
      beast::bind_front_handler(&Session::onHandshake, shared_from_this()));
}

void Session::onHandshake(beast::error_code ec) {
  if (ec) {
    std::cerr << "[Discord] Handshake failed: " << ec.message() << '\n';
    return;
  }

  ws.async_read(buffer,
      beast::bind_front_handler(&Session::onRead, shared_from_this()));
}

void Session::onRead(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    std::cerr << "[Discord] Read failed: " << ec.message() << '\n';
    return;
  }

  auto response = json::parse(beast::buffers_to_string(buffer.data()));
  buffer.clear();

  handler(response);
  
  ws.async_read(buffer,
      beast::bind_front_handler(&Session::onRead, shared_from_this()));
}

void Session::doWrite() {
  ws.async_write(asio::buffer(writeQueue.front()),
      beast::bind_front_handler(&Session::onWrite, shared_from_this()));
}

void Session::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
  if (ec) {
    std::cerr << "[Discord] Write failed: " << ec.message() << '\n';
    return;
  }

  writeQueue.pop_front();

  if (!writeQueue.empty())
    doWrite();
}

void Session::onClose(beast::error_code ec) {
  if (ec == ssl::error::stream_truncated) {
    ec = {};
  }

  if (ec) {
    std::cerr << "[Discord] Close failed: " << ec.message() << '\n';
    return;
  }
}

} // namespace discord

} // namespace dc
