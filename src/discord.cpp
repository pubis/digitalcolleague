#include "discord.hpp"

namespace dc {

namespace discord {

settings tag_invoke(json::value_to_tag<settings>, const json::value& jv) {
  settings s;
  const json::object& obj = jv.as_object();
  extract(obj, s.enabled, "enabled");
  extract(obj, s.token, "token");
  return s;
}

session_start_limit_t tag_invoke(json::value_to_tag<session_start_limit_t>, const json::value& jv) {
  session_start_limit_t s;
  const json::object& obj = jv.as_object();
  extract(obj, s.total, "total");
  extract(obj, s.remaining, "remaining");
  extract(obj, s.reset_after, "reset_after");
  extract(obj, s.max_concurrency, "max_concurrency");
  return s;
}

gateway tag_invoke(json::value_to_tag<gateway>, const json::value& jv) {
  gateway g;
  const json::object& obj = jv.as_object();
  extract(obj, g.url, "url");
  extract(obj, g.shards, "shards");
  extract(obj, g.session_start_limit, "session_start_limit");
  return g;
}

OpCode tag_invoke(json::value_to_tag<OpCode>, const json::value& jv) {
  return static_cast<OpCode>(json::value_to<int>(jv));
}

void session::run() {
  try {
    get_gateway();
  } catch (const std::exception& e) {
    std::cerr << "[Discord] Failed to get gateway: " << e.what() << '\n';
    return;
  }

  connect();
}

void session::connect() {
  host_ = gateway_.url.substr(gateway_.url.find_last_of('/') + 1);
  std::cout << "[Discord] Connecting to: " << host_ << '\n';

  resolver_.async_resolve(host_, "443",
      beast::bind_front_handler(&session::on_resolve, shared_from_this()));
}

void session::disconnect() {
  std::cout << "[Discord] Disconnecting\n";

  ws_.async_close(ws::close_code::normal,
      beast::bind_front_handler(&session::on_close, shared_from_this()));
}

void session::reconnect() {
  std::cout << "[Discord] Reconnecting\n";

  disconnect();
  connect();
}

void session::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
  if (ec) {
    std::cerr << "[Discord] Resolve failed: " << ec.message() << '\n';
    return;
  }

  beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

  beast::get_lowest_layer(ws_).async_connect(results,
      beast::bind_front_handler(&session::on_connect, shared_from_this()));
}

void session::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
  if (ec) {
    std::cerr << "[Discord] Connect failed: " << ec.message() << '\n';
    return;
  }

  host_ += ':' + std::to_string(endpoint.port());

  beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

  if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
    ec = beast::error_code(static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category());
    std::cerr << "[Discord] SSL Error: " << ec.message() << '\n';
    return;
  }

  ws_.next_layer().async_handshake(ssl::stream_base::client,
      beast::bind_front_handler(&session::on_ssl_handshake, shared_from_this()));
}

void session::on_ssl_handshake(beast::error_code ec) {
  if (ec) {
    std::cerr << "[Discord] SSL Handshake failed: " << ec.message() << '\n';
    return;
  }

  beast::get_lowest_layer(ws_).expires_never();

  ws_.set_option(ws::stream_base::timeout::suggested(beast::role_type::client));

  ws_.set_option(ws::stream_base::decorator(
        [](ws::request_type& req) {
          req.set(http::field::user_agent, "DiscordBot (https://github.com/pubis, 1.0)");
        }
  ));

  ws_.async_handshake(host_, "/?v=6&encoding=json",
      beast::bind_front_handler(&session::on_handshake, shared_from_this()));
}

void session::on_handshake(beast::error_code ec) {
  if (ec) {
    std::cerr << "[Discord] Handshake failed: " << ec.message() << '\n';
    return;
  }

  ws_.async_read(buffer_,
      beast::bind_front_handler(&session::on_read, shared_from_this()));
}

void session::on_read(beast::error_code ec, std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    std::cerr << "[Discord] Read failed: " << ec.message() << '\n';
    return;
  }

  auto response = beast::buffers_to_string(buffer_.data());
  buffer_.clear();

  try {
    auto json_response = json::parse(response);
    OpCode op = json::value_to<OpCode>(json_response.at("op"));

    switch (op) {
      case OpCode::Dispatch: {
        sequence_ = json::value_to<int>(json_response.at("s"));
        std::string event = json::value_to<std::string>(json_response.at("t"));
        auto data = json_response.at("d");

        on_dispatch(event, data);
      } break;
      case OpCode::Heartbeat:
      case OpCode::Identify:
      case OpCode::PresenceUpdate:
      case OpCode::VoiceStateUpdate:
      case OpCode::Resume:
      case OpCode::Reconnect: {
        reconnect();
        return;
      } break;
      case OpCode::RequestGuildMembers:
      case OpCode::InvalidSession:
        std::cout << "[Discord] OpCode not implemented (" << json_response.at("op") << ")\n";
        std::cout << "[Discord] Payload:\n" << json_response << '\n';
        break;
      case OpCode::Hello: {
        on_hello(json_response.at("d"));
      } break;
      case OpCode::HeartbeatAck: {
        needAck_--;
        if (needAck_ < 0) {
          std::cerr << "[Discord] Received extra Heartbeat Ack\n";
        }
      } break;
      default: {
        std::cout << "[Discord] Unknown op: " << json_response.at("op") << '\n';
        std::cout << "[Discord] Payload:\n" << json_response << '\n';
      } break;
    }

  } catch (const std::exception& e) {
    std::cout << "[Discord] Failed to parse response JSON: " << e.what() << '\n'
      << "Response: " << response << '\n';
  }

  ws_.async_read(buffer_,
      beast::bind_front_handler(&session::on_read, shared_from_this()));
}

void session::on_dispatch(const std::string& event, const json::value& data) {
  if (event == "READY") {
    identified_ = true;
    session_id_ = json::value_to<std::string>(data.at("session_id"));
    std::cout << "[Discord] Identified. Payload: " << data << '\n';
  } else if (event == "RESUMED") {
    std::cout << "[Discord] Resumed. Payload: " << data << '\n';
  } else {
    std::cout << "[Discord] Unhandled dispatch event `" << event << "`, payload: " << data << '\n';
  }
}

void session::on_hello(const json::value& d) {
  std::cout << "[Discord] Hello\n";

  heartrate_ = std::chrono::milliseconds(
      json::value_to<int>(d.at("heartbeat_interval")));

  send_heartbeat(boost::system::errc::make_error_code(
      boost::system::errc::success));

  if (!identified_) {
    send_identify();
  } else {
    send_resume();
  }
}

void session::send_heartbeat(const boost::system::error_code& ec) {
  if (ec) {
    return;
  }

  if (needAck_ > 1) {
    std::cerr << "[Discord] Server did not respond to " << needAck_
      << " heartbeats. Disconnecting.\n";
    disconnect();
  }

  timer_ = std::make_unique<boost::asio::steady_timer>(
      io_, std::chrono::steady_clock::now() + heartrate_);
  timer_->async_wait(
      [this](const boost::system::error_code& ec) {
        send_heartbeat(ec);
      }
  );

  std::cout << "[Discord] Sending heartbeat. Sequence " << sequence_ << '\n';

  needAck_++;

  if (sequence_ >= 0) {
    send(1, sequence_);
  } else {
    send(1, nullptr);
  }
}

void session::send_identify() {
  const int intents = GUILD_MESSAGES | DIRECT_MESSAGES;

  json::object data;
  data["token"] = settings_.token;
  data["intents"] = intents;
  data["properties"] = {
    { "$os", "linux" },
    { "$browser", "DigitalColleague" },
    { "$device", "DigialColleague" }
  };
  data["compress"] = false;
  data["presence"] = {
    { "activities", nullptr },
    { "status", "online" },
    { "since", nullptr },
    { "afk", false }
  };

  std::cout << "[Discord] Identifying\n";

  send(2, data);
}

void session::send_resume() {
  json::object data;
  data["token"] = settings_.token;
  data["session_id"] = session_id_;
  data["seq"] = sequence_;

  std::cout << "[Discord] Resuming\n";

  send(6, data);
}

void session::send(int opcode, const json::value& data) {
  json::object obj;
  obj["op"] = opcode;
  obj["d"] = data;

  bool write_in_progress = !write_queue_.empty();

  write_queue_.push_back(json::serialize(obj));

  if (!write_in_progress)
    do_write();
}

void session::do_write() {
  ws_.async_write(asio::buffer(write_queue_.front()),
      beast::bind_front_handler(&session::on_write, shared_from_this()));
}

void session::on_write(beast::error_code ec, std::size_t bytes_transferred) {
  if (ec) {
    std::cerr << "[Discord] Write failed: " << ec.message() << '\n';
    return;
  }

  write_queue_.pop_front();

  if (!write_queue_.empty())
    do_write();
}

void session::on_close(beast::error_code ec) {
  if (ec == ssl::error::stream_truncated) {
    ec = {};
  }

  if (ec) {
    std::cerr << "[Discord] Close failed: " << ec.message() << '\n';
    return;
  }

  std::cout << "[Discord] Results:\n" << beast::make_printable(buffer_.data()) << '\n';
}

void session::request(http::verb method, const std::string& endpoint, const json::object& payload) {
  const char* host = "discord.com";
  const char* port = "443";
  std::string target = "/api/v8/";

  tcp::resolver resolver{ io_ };
  beast::ssl_stream<beast::tcp_stream> stream(io_, ctx_);

  if (!SSL_set_tlsext_host_name(stream.native_handle(), host)) {
    beast::error_code ec{ static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category() };
    throw beast::system_error{ ec };
  }

  auto const results = resolver.resolve(host, port);

  beast::get_lowest_layer(stream).connect(results);

  stream.handshake(ssl::stream_base::client);

  target += endpoint;
  std::string data = json::serialize(payload);

  std::cout << "[Discord] Request target: " << target << ", data: " << data << '\n';

  http::request<http::string_body> request{ method, target, 11 };
  request.set(http::field::host, host);
  request.set(http::field::user_agent, "DiscordBot (https://github.com/pubis, 0.1)");
  request.set(http::field::authorization, "Bot " + settings_.token);
  request.set(http::field::content_type, "application/json");
  request.body() = data;
  request.set(http::field::content_length, std::to_string(data.length()));

  http::write(stream, request);

  beast::flat_buffer buffer;

  http::response<http::string_body> response;

  http::read(stream, buffer, response);

  auto json_response = json::parse(response.body());

  std::cout << "[Discord] Reponse: " << json_response << '\n';

  beast::error_code ec;
  stream.shutdown(ec);
  if (ec == asio::error::eof || ec == ssl::error::stream_truncated) {
    ec = {};
  }
  if (ec) {
    std::cerr << "[Discord] Request failed: " << ec.message() << '\n';
  }
}

void session::get_gateway() {
  const char* host = "discord.com";
  const char* port = "443";
  const char* target = "/api/v8/gateway/bot";

  std::cout << "[Discord] Retrieving gateway\n";

  tcp::resolver resolver{ io_ };
  beast::ssl_stream<beast::tcp_stream> stream(io_, ctx_);

  if (!SSL_set_tlsext_host_name(stream.native_handle(), host)) {
    beast::error_code ec{ static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category() };
    throw beast::system_error{ ec };
  }

  auto const results = resolver.resolve(host, port);

  beast::get_lowest_layer(stream).connect(results);

  stream.handshake(ssl::stream_base::client);

  http::request<http::empty_body> request{ http::verb::get, target, 11 };
  request.set(http::field::host, host);
  request.set(http::field::user_agent, "DiscordBot (https://github.com/pubis, 0.1)");
  request.set(http::field::authorization, "Bot " + settings_.token);
  request.set(http::field::content_length, "0");

  http::write(stream, request);

  beast::flat_buffer buffer;

  http::response<http::string_body> response;

  http::read(stream, buffer, response);

  gateway_ = json::value_to<gateway>(json::parse(response.body()));

  std::cout << "[Discord] Gateway: " << gateway_.url << '\n';

  beast::error_code ec;
  stream.shutdown(ec);
  if (ec == asio::error::eof || ec == ssl::error::stream_truncated) {
    ec = {};
  }
  if (ec)
    throw beast::system_error(ec);
}

} // namespace discord

} // namespace dc
