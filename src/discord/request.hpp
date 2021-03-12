#pragma once

#include "../common.hpp"

namespace dc {

namespace discord {

class Request : public std::enable_shared_from_this<Request> {
  using callback = std::function<void(const beast::error_code& ec, const json::value& data)>;

  tcp::resolver resolver;
  beast::ssl_stream<beast::tcp_stream> stream;
  beast::flat_buffer buffer;
  http::request<http::string_body> request;
  http::response<http::string_body> response;
  std::string host;
  std::string port{ "443" };
  int version{ 11 };
  std::string token;
  callback handler;

public:
  explicit Request(asio::any_io_executor ex, ssl::context& ctx, std::string host, std::string token)
    : resolver(ex)
    , stream(ex, ctx)
    , host(std::move(host))
    , token(std::move(token))
  {}

  void get(const std::string& endpoint, callback handler);
  void post(const std::string& endpoint, const std::string& payload, callback handler);

private:
  void run(http::verb method, const std::string& target, const std::string& payload);

  void onResolve(beast::error_code ec, tcp::resolver::results_type results);
  void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint);
  void onHandshake(beast::error_code ec);
  void onWrite(beast::error_code ec, std::size_t bytes);
  void onRead(beast::error_code ec, std::size_t bytes);
  void onShutdown(beast::error_code ec);

};

} // namespace discord

} // namespace dc
