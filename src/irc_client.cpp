#include "irc_client.hpp"

#include <iostream>

using std::placeholders::_1;
using std::placeholders::_2;

namespace irc {

settings tag_invoke(json::value_to_tag<settings>, const json::value& jv) {
  settings s;
  const json::object& obj = jv.as_object();
  extract(obj, s.host, "host");
  extract(obj, s.port, "port");
  extract(obj, s.nick, "nick");
  extract(obj, s.pass, "pass");
  extract(obj, s.channels, "channels");
  return s;
}

client::client(asio::io_context& io, ssl::context& ctx, const irc::settings& settings)
  : io(io)
  , ctx(ctx)
  , settings(settings)
  , socket(io, ctx)
{
  socket.set_verify_mode(ssl::verify_peer);
  socket.set_verify_callback(std::bind(&client::verify_certificate, this, _1, _2));

  register_handler(
    "PING",
    [this](auto, auto, std::string_view ping) {
      std::stringstream pong;
      pong << "PONG :" << ping;
      send_line(pong.str());
      std::cout << "> PONG\n";
    }
  );

  connect();
}

void client::join(std::string_view channel) {
  std::stringstream msg;
  msg << "JOIN " << channel;
  send_line(msg.str());
  std::cout << "> " << msg.str() << '\n';
}

void client::say(std::string_view receiver, std::string_view message) {
  std::stringstream msg;
  msg << "PRIVMSG " << receiver << " :" << message;
  send_line(msg.str());
  std::cout << "> " << msg.str() << '\n';
}

void client::send_line(std::string data) {
  data += "\r\n";
  to_write.push_back(std::move(data));

  if (to_write.size() == 1)
    send_raw();
}

void client::register_handler(std::string name, message_handler handler) {
  handlers[std::move(name)].push_back(handler);
}

void client::connect() {
  //socket.shutdown();
  tcp::resolver resolver{ io };

  auto handler = [this](auto&&... params) {
    on_hostname_resolved(std::forward<decltype(params)>(params)...);
  };

  resolver.async_resolve(settings.host, std::to_string(settings.port), handler);
}

void client::identify() {
  std::stringstream msg;
  msg << "PASS " << settings.pass;
  send_line(msg.str());
  std::cout << "> PASS ********\n";

  msg.str("");
  msg << "NICK " << settings.nick;
  send_line(msg.str());
  std::cout << "> " << msg.str() << '\n';
}

void client::on_hostname_resolved(const boost::system::error_code& error, tcp::resolver::results_type results) {
  if (error) {
    connect();
    return;
  }

  if (!results.size()) {
    std::stringstream msg;
    msg << "Failed to resolve '" << settings.host << "'";
    throw std::runtime_error(msg.str());
  }

  asio::async_connect(socket.lowest_layer(), results,
      [this](const boost::system::error_code& error, const tcp::endpoint& /* endpoint */) {
        on_connected(error);
      }
  );
}

void client::on_connected(const boost::system::error_code& error) {
  if (error) {
    std::cerr << "[IRC] Connect error: " << error.message() << '\n';
    connect();
    return;
  }

  std::cout << "Connected.\n";

  socket.async_handshake(ssl::stream_base::client,
      [this](const boost::system::error_code& error) {
        on_handshake(error);
      }
  );
}

void client::on_handshake(const boost::system::error_code& error) {
  if (error) {
    std::cerr << "[IRC] Handshake failed: " << error.message() << '\n';
    connect();
    return;
  }

  identify();

  await_new_line();
}

bool client::verify_certificate(bool preverified, ssl::verify_context& ctx) {
  std::string subject(256, '\0');
  X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
  X509_NAME_oneline(X509_get_subject_name(cert), &subject[0], 256);
  std::cout << "Verifying " << subject << '\n';

  return preverified;
}

void client::await_new_line() {
  auto handler = [this](const auto& error, std::size_t s) {
    if (error) {
      std::cerr << "[IRC] Read error: " << error.message() << '\n';
      connect();
      return;
    }

    std::istream istrm{ &in_buf };
    std::string line;
    std::getline(istrm, line);

    on_new_line(line);
    await_new_line();
  };

  asio::async_read_until(socket, in_buf, "\r\n", handler);
}

void client::on_new_line(const std::string& line) {
  std::cout << "< " << line << '\n';

  static auto constexpr server_message =
    R"((?::([^@!\ ]*(?:(?:![^@]*)?@[^\ ]*)?)\ ))"
    R"(?([^\ ]+)((?:[^:\ ][^\ ]*)?(?:\ [^:\ ][^\ ]*))"
    R"({0,14})(?:\ :?(.*))?)";

  std::string who, type, where, message;
  extract_regex_groups(line.c_str(), std::regex{ server_message }, std::tie(who, type, where, message));

  handle_message(who, type, where, message);
}

void client::handle_message(std::string_view who, const std::string& type, std::string_view where, std::string_view message) {
  for (const auto& handler: handlers[type]) {
    handler(who, where, message);
  }
}

void client::send_raw() {
  if (to_write.empty())
    return;

  asio::async_write(socket, asio::buffer(to_write.front().data(), to_write.front().size()),
    [this](auto&&... params) {
      handle_write(params...);
    }
  );
}

void client::handle_write(const boost::system::error_code& error, std::size_t bytes_read) {
  if (error) {
    std::cerr << "[IRC] Write error: " << error << '\n';
    return;
  }

  auto to_erase = std::min(bytes_read, to_write.front().size());
  auto& buf = to_write.front();

  buf.erase(buf.begin(), buf.begin() + to_erase);

  if (buf.empty())
    to_write.erase(to_write.begin());

  if (!to_write.empty())
    send_raw();
}

}

