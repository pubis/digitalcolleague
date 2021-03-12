#pragma once

#include "../common.hpp"
#include "gateway.hpp"

namespace dc {

namespace discord {

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

OpCode tag_invoke(json::value_to_tag<OpCode>, const json::value& jv);

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

class Session : public std::enable_shared_from_this<Session> {
  using Stream = ws::stream<beast::ssl_stream<beast::tcp_stream>>;
  using callback = std::function<void(const json::value& data)>;

  tcp::resolver resolver;
  Stream ws;
  beast::flat_buffer buffer;
  std::deque<std::string> writeQueue;
  std::string host;
  callback handler;

public:
  explicit Session(asio::io_context& io, ssl::context& ctx)
    : resolver(asio::make_strand(io))
    , ws(asio::make_strand(io), ctx)
  {}

  void run(const Gateway& gateway, callback handler);
  void connect(const Gateway& gateway);
  void send(const json::object& data);
  void disconnect();

private:
  void onResolve(beast::error_code ec, tcp::resolver::results_type results);
  void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint);
  void onSslHandshake(beast::error_code ec);
  void onHandshake(beast::error_code ec);
  void onRead(beast::error_code ec, std::size_t bytes);
  void doWrite();
  void onWrite(beast::error_code ec, std::size_t bytes);
  void onClose(beast::error_code ec);

};

} // namespace discord

} // namespace dc
