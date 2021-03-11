#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/json.hpp>

namespace dc {

namespace discord {

namespace json  = boost::json;
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ssl   = asio::ssl;
namespace ws    = beast::websocket;

using tcp = asio::ip::tcp;

struct settings {
  bool enabled;
  std::string token;
};

struct session_start_limit_t {
  int total;
  int remaining;
  int reset_after;
  int max_concurrency;
};

struct gateway {
  std::string url;
  int shards;
  session_start_limit_t session_start_limit;
};

template <class T>
void extract(const json::object& obj, T& t, json::string_view key) {
  t = json::value_to<T>(obj.at(key));
}

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

enum class OpCode {
  Dispatch            = 0,
  Heartbeat           = 1,
  Identify            = 2,
  PresenceUpdate      = 3,
  VoiceStateUpdate    = 4,
  Resume              = 6,
  Reconnect           = 7,
  RequestGuildMembers = 8,
  InvalidSession      = 9,
  Hello               = 10,
  HeartbeatAck        = 11,
};

const int GUILDS                    = 1 << 0;
const int GUILD_MEMBERS             = 1 << 1;
const int GUILD_BANS                = 1 << 2;
const int GUILD_EMOJIS              = 1 << 3;
const int GUILD_INTEGRATIONS        = 1 << 4;
const int GUILD_WEBHOOKS            = 1 << 5;
const int GUILD_INVITES             = 1 << 6;
const int GUILD_VOICE_STATES        = 1 << 7;
const int GUILD_PRESENCES           = 1 << 8;
const int GUILD_MESSAGES            = 1 << 9;
const int GUILD_MESSAGE_REACTIONS   = 1 << 10;
const int GUILD_MESSAGE_TYPING      = 1 << 11;
const int DIRECT_MESSAGES           = 1 << 12;
const int DIRECT_MESSAGE_REACTIONS  = 1 << 13;
const int DIRECT_MESSAGE_TYPING     = 1 << 14;

OpCode tag_invoke(json::value_to_tag<OpCode>, const json::value& jv) {
  return static_cast<OpCode>(json::value_to<int>(jv));
}

class session : public std::enable_shared_from_this<session> {
  asio::io_context& io_;
  ssl::context& ctx_;
  tcp::resolver resolver_;
  ws::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
  beast::flat_buffer buffer_;
  settings settings_;
  std::string host_;
  gateway gateway_;
  std::chrono::milliseconds heartrate_{ 0 };
  int needAck_{ 0 };
  int sequence_{ -1 };
  std::unique_ptr<boost::asio::steady_timer> timer_;
  bool identified_{ false };
  std::string session_id_;

public:
  explicit session(asio::io_context& io, ssl::context& ctx, const settings& s)
    : io_(io)
    , ctx_(ctx)
    , resolver_(asio::make_strand(io_))
    , ws_(asio::make_strand(io_), ctx_)
    , settings_(s)
  {}

  void run() {
    try {
      get_gateway();
    } catch (const std::exception& e) {
      std::cerr << "[Discord] Failed to get gateway: " << e.what() << '\n';
      return;
    }

    host_ = gateway_.url.substr(gateway_.url.find_last_of('/') + 1);
    std::cout << "[Discord] host: " << host_ << '\n';

    resolver_.async_resolve(host_, "443",
        beast::bind_front_handler(&session::on_resolve, shared_from_this()));
  }

  void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
      std::cerr << "[Discord] Resolve failed: " << ec.message() << '\n';
      return;
    }

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    beast::get_lowest_layer(ws_).async_connect(results,
        beast::bind_front_handler(&session::on_connect, shared_from_this()));
  }

  void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint) {
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

  void on_ssl_handshake(beast::error_code ec) {
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

  void on_handshake(beast::error_code ec) {
    if (ec) {
      std::cerr << "[Discord] Handshake failed: " << ec.message() << '\n';
      return;
    }

    ws_.async_read(buffer_,
        beast::bind_front_handler(&session::on_read, shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
      std::cerr << "[Discord] Read failed: " << ec.message() << '\n';
      return;
    }

    auto response = beast::buffers_to_string(buffer_.data());
    try {
      auto json_response = json::parse(response);
      OpCode op = json::value_to<OpCode>(json_response.at("op"));

      switch (op) {
        case OpCode::Dispatch: {
          sequence_ = json::value_to<int>(json_response.at("s"));
          std::string event = json::value_to<std::string>(json_response.at("t"));
          auto data = json_response.at("d");

          if (event == "READY") {
            identified_ = true;
            session_id_ = json::value_to<std::string>(data.at("session_id"));
          }

          std::cout << "[Discord] Dispatch event payload:\n" << data << '\n';
        } break;
        case OpCode::Heartbeat:
        case OpCode::Identify:
        case OpCode::PresenceUpdate:
        case OpCode::VoiceStateUpdate:
        case OpCode::Resume:
        case OpCode::Reconnect:
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
          if (!identified_) {
            send_identify();
            identified_ = true; // Move to Ready event
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

    buffer_.clear();

    ws_.async_read(buffer_,
        beast::bind_front_handler(&session::on_read, shared_from_this()));
  }

  void disconnect() {
    ws_.async_close(ws::close_code::normal,
        beast::bind_front_handler(&session::on_close, shared_from_this()));
  }

  void on_hello(const json::value& d) {
    std::cout << "[Discord] Hello\n";

    heartrate_ = std::chrono::milliseconds(
        json::value_to<int>(d.at("heartbeat_interval")));

    send_heartbeat(boost::system::errc::make_error_code(
          boost::system::errc::success));
  }

  void send_heartbeat(const boost::system::error_code& ec) {
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

  void send_identify() {
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

    send(2, data);
  }

  void send(int opcode, const json::value& data) {
    json::object obj;
    obj["op"] = opcode;
    obj["d"] = data;

    std::string payload = json::serialize(obj);
    ws_.async_write(asio::buffer(payload),
        beast::bind_front_handler(&session::on_write, shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
      std::cerr << "[Discord] Write failed: " << ec.message() << '\n';
      return;
    }
  }

  void on_close(beast::error_code ec) {
    if (ec == ssl::error::stream_truncated) {
      ec = {};
    }

    if (ec) {
      std::cerr << "[Discord] Close failed: " << ec.message() << '\n';
      return;
    }

    std::cout << "[Discord] Results:\n" << beast::make_printable(buffer_.data()) << '\n';
  }

private:
  void get_gateway() {
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

};

} // namespace discord

} // namespace dc
