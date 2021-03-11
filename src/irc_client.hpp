#pragma once

#include "common.hpp"

namespace detail {

template <typename Tuple, size_t... Is>
void assign_regex_matches(Tuple&& tup, const std::cmatch& match, std::index_sequence<Is...>) {
  using tuple_t = std::decay_t<Tuple>;
  (
  (void) (
    std::get<Is>(tup) =
      boost::lexical_cast<
        std::decay_t<
          std::tuple_element_t<Is, tuple_t>
        >
      >(match[1 + Is])
  ), ...
  );
}

}

template <typename Tuple>
void extract_regex_groups(const char* string, const std::regex& regex, Tuple&& tuple) {
  constexpr auto size = std::tuple_size<Tuple>{}();

  std::cmatch match;
  std::regex_search(string, match, regex);

  if (match.size() != size + 1)
    throw std::runtime_error("Wrong number of captures");

  detail::assign_regex_matches(std::forward<Tuple>(tuple), match, std::make_index_sequence<size>{});
}

namespace dc {

namespace irc {

struct settings {
  bool enabled;
  std::string host;
  int port;
  std::string nick;
  std::string pass;
  std::vector<std::string> channels;
};

settings tag_invoke(json::value_to_tag<settings>, const json::value& jv);

class client {
  using tcp = asio::ip::tcp;

public:
  using message_handler = std::function<void(std::string_view, std::string_view, std::string_view)>;
  using ssl_socket = ssl::stream<tcp::socket>;

private:
  asio::io_context& io;
  ssl::context& ctx;
  irc::settings settings;
  //tcp::socket socket;
  ssl_socket socket;
  asio::streambuf in_buf;
  std::unordered_map<std::string, std::vector<message_handler>> handlers;
  std::deque<std::string> to_write;

public:
  client(asio::io_context& io, ssl::context& ctx, const irc::settings& settings);

  void join(std::string_view channel);
  void say(std::string_view receiver, std::string_view message);

  void send_line(std::string data);
  void register_handler(std::string name, message_handler handler);

  const auto& get_settings() const { return settings; }

private:
  void connect();
  void identify();

  void on_hostname_resolved(const boost::system::error_code& error, tcp::resolver::results_type results);
  void on_connected(const boost::system::error_code& error);
  void on_handshake(const boost::system::error_code& error);
  bool verify_certificate(bool preverified, ssl::verify_context& ctx);
  void await_new_line();
  void on_new_line(const std::string& line);
  void handle_message(std::string_view who, const std::string& type, std::string_view where, std::string_view message);
  void send_raw();
  void handle_write(const boost::system::error_code& error, std::size_t bytes_read);
};

} // namespace irc

} // namespace dc
