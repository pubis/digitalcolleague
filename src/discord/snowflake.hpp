#pragma once

#include "../common.hpp"

namespace dc {

namespace discord {

static constexpr uint64_t DiscordEpoch{ 1420070400000 };

struct Snowflake {
  uint64_t id;

  uint64_t timestamp() const {
    return (id >> 22) + DiscordEpoch;
  }

  uint32_t workerId() const {
    return (id & 0x3e0000) >> 17;
  }

  uint32_t processId() const {
    return (id & 0x1f000) >> 12;
  }

  uint32_t increment() const {
    return id & 0xfff;
  }

};

Snowflake tag_invoke(json::value_to_tag<Snowflake>, const json::value& jv);

} // namespace discord

} // namespace dc
