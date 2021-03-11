#pragma once

#include "common.hpp"

namespace dc {

namespace discord {

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

settings tag_invoke(json::value_to_tag<settings>, const json::value& jv);
session_start_limit_t tag_invoke(json::value_to_tag<session_start_limit_t>, const json::value& jv);
gateway tag_invoke(json::value_to_tag<gateway>, const json::value& jv);

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

OpCode tag_invoke(json::value_to_tag<OpCode>, const json::value& jv);

class session : public std::enable_shared_from_this<session> {
  asio::io_context& io_;
  ssl::context& ctx_;
  tcp::resolver resolver_;
  ws::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
  beast::flat_buffer buffer_;
  std::deque<std::string> write_queue_;
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

  void run();
  void connect();
  void disconnect();
  void reconnect();

  void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
  void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint);
  void on_ssl_handshake(beast::error_code ec);
  void on_handshake(beast::error_code ec);
  void on_read(beast::error_code ec, std::size_t bytes_transferred);

  void on_dispatch(const std::string& event, const json::value& data);
  void on_hello(const json::value& d);

  void send_heartbeat(const boost::system::error_code& ec);
  void send_identify();
  void send_resume();
  void send(int opcode, const json::value& data);

  void do_write();
  void on_write(beast::error_code ec, std::size_t bytes_transferred);
  void on_close(beast::error_code ec);

  void request(http::verb method, const std::string& endpoint, const json::object& payload);

private:
  void get_gateway();

};

} // namespace discord

} // namespace dc
