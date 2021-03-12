#pragma once

#include "request.hpp"
#include "session.hpp"

namespace dc {

namespace discord {

struct Settings {
  bool enabled;
  std::string token;
};

Settings tag_invoke(json::value_to_tag<Settings>, const json::value& jv);

class Bot {
  asio::io_context& io;
  ssl::context& ctx;
  Settings settings;
  std::optional<Gateway> gateway;
  std::shared_ptr<Session> session;
  std::string session_id;
  bool identified{ false };

  std::unique_ptr<boost::asio::steady_timer> heartbeat;
  std::chrono::milliseconds heartrate;
  int needAck{ 0 };
  int sequence{ -1 };

public:
  Bot(asio::io_context& io, ssl::context& ctx, const Settings& settings)
    : io(io)
    , ctx(ctx)
    , settings(settings)
  {}

  void run();

  void createChannelMessage(size_t channel, const std::string& message);

private:
  void onSessionData(const json::value& data);

  void onDispatch(const std::string& event, const json::value& data);
  void onHello(int heartbeatInterval);

  void sendHeartbeat(const boost::system::error_code& ec);
  void sendIdentify();
  void sendResume();
  void send(OpCode op, const json::value& data);

  void updateGateway();
  void onGatewayUpdated(const beast::error_code& ec, const json::value& data);
};

} // namespace discord

} // namespace dc
