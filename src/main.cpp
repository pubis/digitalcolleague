#include <cstdlib>
#include <csignal>
#include <fstream>
#include <iomanip>

#include <boost/algorithm/string/trim.hpp>

#include <sqlite3.h>

#include <aegis.hpp>

#include "twitch.hpp"
#include "console.hpp"

using namespace dc;

std::shared_ptr<asio::io_context> io{ nullptr };

void signal_handler(int sig) {
  std::cout << "Caught Ctrl-C, stopping io context event loop...\n";

  io->stop();
}

json::value read_json_file(const char* file, boost::system::error_code& error) {
  std::ifstream is{ file };
  json::stream_parser p;
  std::string line;

  while (std::getline(is, line)) {
    p.write(line, error);
    if (error)
      return nullptr;
  }
  p.finish(error);
  if (error)
    return nullptr;
  return p.release();
}

void greet(twitch::client& client, std::string_view who, std::string_view where, std::string_view message) {
  std::string nick;

  extract_regex_groups(who.data(), std::regex{ "([^!:]+)" }, std::tie(nick));
  boost::algorithm::trim(nick);

  std::cout << "NICK: " << nick << '\n';

  if (nick == client.get_settings().nick) {
    return;
  }

  std::string dest{ where };
  boost::algorithm::trim(dest);
  client.say(dest, "Hello, " + nick + "!");
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <config> <database>\n";
    return EXIT_FAILURE;
  }

  boost::system::error_code error;
  auto secret = read_json_file(argv[1], error);
  if (error) {
    std::cerr << "Failed to read config: " << error.message() << '\n';
    return EXIT_FAILURE;
  }

  dc::twitch::settings settings{ json::value_to<dc::twitch::settings>(secret.at("twitch")) };

  std::cout << "SQLite threadsafe: " << sqlite3_threadsafe() << '\n';
  sqlite3 *db{ nullptr };
  auto rc = sqlite3_open(argv[2], &db);
  if (rc) {
    std::cerr << "Can' open database: " << sqlite3_errmsg(db) << '\n';
    sqlite3_close(db);
    return EXIT_FAILURE;
  } else {
    std::cout << "Database: " << argv[2] << '\n';
  }

  using asio::ip::tcp;
  io = std::make_shared<asio::io_context>();

  asio::ssl::context ssl_ctx{ asio::ssl::context::tls };
  ssl_ctx.set_default_verify_paths();

  twitch::client twitch{ *io, ssl_ctx, settings };

  twitch.register_handler("001", [&](auto&&...) {
    for (const auto& channel: settings.channels) {
      twitch.join(channel);
    }
  });

  twitch.register_handler("PRIVMSG",
    [&](auto&& who, auto&& where, auto&& message) {
      std::string nick;
      extract_regex_groups(who.data(), std::regex{ "([^!:]+)" }, std::tie(nick));
      boost::algorithm::trim(nick);

      std::string channel{ where };
      boost::algorithm::trim(channel);

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
          now.time_since_epoch()).count();

      std::stringstream sql;
      sql << "INSERT INTO message (timestamp, nick, channel, message) "
          << "VALUES ("
          << timestamp << ", "
          << std::quoted(nick) << ", "
          << std::quoted(channel) << ", "
          << std::quoted(message)
          << ");";
      std::cout << "Executing: " << sql.str() << '\n';

      char *error{ nullptr };
      auto rc = sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, &error);
      if (rc) {
        std::cerr << "SQL Error: " << error << '\n';
      }
    }
  );

  std::signal(SIGINT, signal_handler);

  dc::console::server console{ *io, json::value_to<dc::console::settings>(secret.at("console")) };

  auto token = json::value_to<std::string>(secret.at("discord").at("token"));
  aegis::core discord(aegis::create_bot_t()
      .log_level(spdlog::level::trace)
      .io_context(io)
      .token(token));

  discord.set_on_message_create([&](aegis::gateway::events::message_create obj) {
        std::string content{ obj.msg.get_content() };
        auto& channel = obj.msg.get_channel();
        auto& author = obj.msg.get_user();

        std::cout << "[" << channel.get_name() << "] " << author.get_username() << ": " << content << '\n';
      });

  discord.run();
  io->run();

  std::cout << "Disconnected.\n";

  rc = sqlite3_close(db);
  if (rc == SQLITE_OK)
    std::cout << "Database closed\n";
  else
    std::cerr << "Failed to close database\n";

  return 0;
}
