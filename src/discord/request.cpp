#include "request.hpp"

namespace dc {

namespace discord {

void Request::get(const std::string& endpoint, callback handler) {
  this->handler = handler;

  run(http::verb::get, endpoint, "");
}

void Request::post(const std::string& endpoint, const std::string& payload, callback handler) {
  this->handler = handler;

  run(http::verb::post, endpoint, payload);
}

void Request::run(http::verb method, const std::string& target, const std::string& payload) {
  if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
    beast::error_code ec{ static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category() };
    std::cerr << "[Discord] SSL Error: " << ec.message() << '\n';
    return;
  }

  request.version(version);
  request.method(method);
  request.target(target);
  request.set(http::field::host, host);
  request.set(http::field::user_agent, "DiscordBot (https://github.com/pubis, 0.1)");
  request.set(http::field::authorization, "Bot " + token);
  request.set(http::field::content_type, "application/json");
  request.set(http::field::content_length, std::to_string(payload.length()));
  request.body() = payload;

  resolver.async_resolve(host, port,
      beast::bind_front_handler(&Request::onResolve, shared_from_this()));
}

void Request::onResolve(beast::error_code ec, tcp::resolver::results_type results) {
  if (ec)
    return handler(ec, {});

  beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

  beast::get_lowest_layer(stream).async_connect(results,
      beast::bind_front_handler(&Request::onConnect, shared_from_this()));
}

void Request::onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
  if (ec)
    return handler(ec, {});

  beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

  stream.async_handshake(ssl::stream_base::client,
      beast::bind_front_handler(&Request::onHandshake, shared_from_this()));
}

void Request::onHandshake(beast::error_code ec) {
  if (ec)
    return handler(ec, {});

  beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

  http::async_write(stream, request,
      beast::bind_front_handler(&Request::onWrite, shared_from_this()));
}

void Request::onWrite(beast::error_code ec, std::size_t bytes) {
  boost::ignore_unused(bytes);

  if (ec)
    return handler(ec, {});

  http::async_read(stream, buffer, response,
      beast::bind_front_handler(&Request::onRead, shared_from_this()));
}

void Request::onRead(beast::error_code ec, std::size_t bytes) {
  boost::ignore_unused(bytes);

  if (ec)
    return handler(ec, {});

  beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));

  stream.async_shutdown(
      beast::bind_front_handler(&Request::onShutdown, shared_from_this()));

  return handler(ec, json::parse(response.body()));
}

void Request::onShutdown(beast::error_code ec) {
  if (ec == asio::error::eof || ec == ssl::error::stream_truncated) {
    ec = {};
  }

  if (ec) {
    std::cerr << "[Discord] Request shutdown failed: " << ec.message() << '\n';
    return;
  }
}

} // namespace discord

} // namespace dc
