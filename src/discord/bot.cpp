#include "bot.hpp"

namespace dc {

namespace discord {

Settings tag_invoke(json::value_to_tag<Settings>, const json::value& jv) {
  Settings s;
  const auto& object = jv.as_object();
  extract(object, s.enabled, "enabled");
  extract(object, s.token, "token");
  return s;
}

void Bot::run() {
  using boost::placeholders::_1;

  if (!settings.enabled)
    return;

  session = std::make_shared<Session>(io, ctx);

  if (!gateway)
    updateGateway();
  else
    session->run(*gateway, boost::bind(&Bot::onSessionData, this, _1));
}

void Bot::createChannelMessage(size_t channel, const std::string& message) {
  auto request = std::make_shared<Request>(asio::make_strand(io), ctx, "discord.com", settings.token);

  std::string endpoint{ "/api/v8/channels/" };
  endpoint += std::to_string(channel);
  endpoint += "/messages";

  json::object payload{
    { "content", message }
  };

  request->post(endpoint, json::serialize(payload),
      [this](const beast::error_code& ec, const json::value& response) {
        if (ec) {
          std::cerr << "[Discord] Failed to create message\n";
        }

        std::cout << "[Discord] Message create response: " << response << '\n';
      });
}

void Bot::onSessionData(const json::value& payload) {
  OpCode op = json::value_to<OpCode>(payload.at("op"));
  auto data  = payload.at("d");

  switch (op) {
    case OpCode::Dispatch: {
      sequence   = json::value_to<int>(payload.at("s"));
      auto event = json::value_to<std::string>(payload.at("t"));

      onDispatch(event, data);
    } break;
    case OpCode::Heartbeat: {
    } break;
    case OpCode::Reconnect: {
      std::cout << "[Discord] Reconnect received\n";
      session->disconnect();
    } break;
    case OpCode::InvalidSession: {
      std::cout << "[Discord] Invalid Session received\n";
      session->disconnect();
    } break;
    case OpCode::Hello: {
      onHello(json::value_to<int>(data.at("heartbeat_interval")));
    } break;
    case OpCode::HeartbeatAck: {
      needAck--;
      if (needAck > 0) {
        std::cerr << "[Discord] Received extra Heartbeat Ack\n";
      }
    } break;
    default: {
      std::cout << "[Discord] Unexpected opcode: " << static_cast<int>(op)
        << ", payload: " << payload << '\n';
    } break;
  }
}

void Bot::onDispatch(const std::string& event, const json::value& data) {
  if (event == "READY") {
    identified = true;
    session_id = json::value_to<std::string>(data.at("session_id"));

    std::cout << "[Discord] Identify payload: " << data << '\n';
  } else if (event == "RESUMED") {
    std::cout << "[Discord] Resume payload: " << data << '\n';
  } else {
    std::cout << "[Discord] Unhandled dispatch event `" << event << "`, payload: " << data << '\n';
  }
}

void Bot::onHello(int heartbeatInterval) {
  std::cout << "[Discord] Hello\n";

  heartrate = std::chrono::milliseconds(heartbeatInterval);

  sendHeartbeat(boost::system::errc::make_error_code(
        boost::system::errc::success));

  if (!identified) {
    sendIdentify();
  } else {
    sendResume();
  }
}

void Bot::sendHeartbeat(const boost::system::error_code& ec) {
  if (ec) {
    std::cerr << "[Discord] Error sending heartbeat: " << ec.message() << '\n';
  }

  heartbeat = std::make_unique<boost::asio::steady_timer>(
      io, std::chrono::steady_clock::now() + heartrate);
  heartbeat->async_wait(
      [this](const boost::system::error_code& ec) {
        sendHeartbeat(ec);
      });

  std::cout << "[Discord] Heartbeat [sequence: " << sequence << "]\n";

  needAck++;

  if (sequence >= 0) {
    send(OpCode::Heartbeat, sequence);
  } else {
    send(OpCode::Heartbeat, nullptr);
  }
}

void Bot::sendIdentify() {
  const int intents = GUILD_MESSAGES | DIRECT_MESSAGES;

  json::object data{
    { "token", settings.token },
    { "intents", intents },
    { "properties", {
      { "$os", "linux" },
      { "$browser", "DigitalColleague" },
      { "$device", "DigitalColleague" }
    }},
    { "compress", false },
    { "presence", {
      { "activities", nullptr },
      { "status", "online" },
      { "since", nullptr },
      { "afk", false }
    }}
  };

  std::cout << "[Discord] Identifying\n";

  send(OpCode::Identify, data);
}

void Bot::sendResume() {
  json::object data{
    { "token", settings.token },
    { "session_id", session_id },
    { "seq", sequence }
  };

  std::cout << "[Discord] Resuming\n";

  send(OpCode::Resume, data);
}

void Bot::send(OpCode op, const json::value& data) {
  json::object payload{
    { "op", static_cast<int>(op) },
    { "d", data }
  };

  session->send(payload);
}

void Bot::updateGateway() {
  using boost::placeholders::_1;
  using boost::placeholders::_2;

  auto request = std::make_shared<Request>(asio::make_strand(io), ctx, "discord.com", settings.token);

  request->get("/api/v8/gateway/bot",
      boost::bind(&Bot::onGatewayUpdated, this, _1, _2));
}

void Bot::onGatewayUpdated(const beast::error_code& ec, const json::value& data) {
  using boost::placeholders::_1;

  if (ec) {
    std::cerr << "[Discord] Failed to get gateway: " << ec.message() << '\n';
    return;
  }

  gateway = json::value_to<Gateway>(data);

  std::cout << "[Discord] Gateway: " << gateway->url << ", shard: " << gateway->shards << '\n';
  std::cout << "[Discord] SessionStartLimit ["
    << "total: " << gateway->sessionStartLimit.total << ", "
    << "remaining: " << gateway->sessionStartLimit.remaining << ", "
    << "resetAfter: " << gateway->sessionStartLimit.resetAfter << ", "
    << "maxConcurrency: " << gateway->sessionStartLimit.maxConcurrency << "]\n";

  session->run(*gateway, boost::bind(&Bot::onSessionData, this, _1));
}

} // namespace discord

} // namespace dc
