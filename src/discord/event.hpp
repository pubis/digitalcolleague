#pragma once

#include <cstdint>
#include <string>

namespace dc {

namespace discord {

/*
 * https://gist.github.com/Lee-R/3839813
 */

namespace detail {

  constexpr std::uint32_t fnv1a_32(char const* s, std::size_t count) {
    return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
  }

} // namespace detail

constexpr std::uint32_t operator"" _hash(char const* s, std::size_t count) {
  return detail::fnv1a_32(s, count);
}

std::uint32_t fnv1a_32(const std::string& s) {
  return detail::fnv1a_32(s.c_str(), s.length());
}

enum class Event: std::uint32_t {
  Ready         = "READY"_hash,
  Resumed       = "RESUMED"_hash,

  MessageCreate = "MESSAGE_CREATE"_hash,
  MessageUpdate = "MESSAGE_UPDATE"_hash,
};

} // namespace discord

} // namespace dc
