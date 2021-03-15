#pragma once

#include <cstdint>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/json.hpp>
#include <boost/lexical_cast.hpp>

namespace dc {

  namespace ssl   = asio::ssl;
  namespace json  = boost::json;

  using tcp = asio::ip::tcp;

  template <class T>
    void extract(const json::object& object, T& t, json::string_view key) {
      t = json::value_to<T>(object.at(key));
    }

  template <class T>
    void extract_maybe(const json::object& object, T& t, json::string_view key, T default_ = T()) {
      if (object.count(key))
        t = json::value_to<T>(object.at(key));
      else
        t = default_;
    }

  template <class T>
    void extract_optional(const json::object& object, std::optional<T>& t, json::string_view key) {
      if (!object.at(key).is_null())
        t = std::make_optional<T>(json::value_to<T>(object.at(key)));
    }

} // namespace dc
