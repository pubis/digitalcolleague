#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/json.hpp>
#include <boost/lexical_cast.hpp>

namespace dc {

  namespace asio  = boost::asio;
  namespace ssl   = asio::ssl;
  namespace beast = boost::beast;
  namespace http  = beast::http;
  namespace ws    = beast::websocket;
  namespace json  = boost::json;

  using tcp = asio::ip::tcp;

} // namespace dc
