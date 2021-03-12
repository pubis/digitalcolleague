#pragma once

#include <cstdint>

namespace dc {

namespace discord {

static constexpr uint64_t DiscordEpoch{ 1420070400000 };

class Snowflake {
  uint64_t snowflake;

public:
  Snowflake(uint64_t snowflake)
    : snowflake(snowflake)
  {}

  uint64_t id() const {
    return snowflake;
  }

  uint64_t timestamp() const {
    return (snowflake >> 22) + DiscordEpoch;
  }

  uint32_t workerId() const {
    return (snowflake & 0x3e0000) >> 17;
  }

  uint32_t processId() const {
    return (snowflake & 0x1f000) >> 12;
  }

  uint32_t increment() const {
    return snowflake & 0xfff;
  }

};

} // namespace discord

} // namespace dc
