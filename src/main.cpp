#include <cstdlib>
#include <csignal>
#include <fstream>
#include <iomanip>

#include <boost/algorithm/string/trim.hpp>

#include <sqlite3.h>

#include "twitch.hpp"
#include "discord/bot.hpp"
#include "console.hpp"

using namespace dc;

asio::io_context io;

void signal_handler(int sig) {
  std::cout << "Caught Ctrl-C, stopping io context event loop...\n";

  io.stop();
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

  twitch::settings settings{ json::value_to<twitch::settings>(secret.at("twitch")) };

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

  using boost::asio::ip::tcp;

  asio::ssl::context ssl_ctx{ asio::ssl::context::tls };
  ssl_ctx.set_default_verify_paths();

  twitch::client twitch{ io, ssl_ctx, settings };

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

  console::server console{ io, json::value_to<console::settings>(secret.at("console")) };

  asio::ssl::context ssl_ctx_discord{ asio::ssl::context::tlsv12_client };
  ssl_ctx_discord.set_default_verify_paths();
  ssl_ctx_discord.set_verify_mode(asio::ssl::verify_peer);

  discord::Bot discord{ io, ssl_ctx_discord, json::value_to<discord::Settings>(secret.at("discord")) };
  discord.run();

  io.run();

  std::cout << "Disconnected.\n";

  rc = sqlite3_close(db);
  if (rc == SQLITE_OK)
    std::cout << "Database closed\n";
  else
    std::cerr << "Failed to close database\n";

  return 0;
}
