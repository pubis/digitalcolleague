#pragma once

#include "../common.hpp"

namespace dc {

namespace discord {

struct SessionStartLimit {
  int total;
  int remaining;
  int resetAfter;
  int maxConcurrency;
};

struct Gateway {
  std::string url;
  int shards;
  SessionStartLimit sessionStartLimit;
};

SessionStartLimit tag_invoke(json::value_to_tag<SessionStartLimit>, const json::value& jv);
Gateway tag_invoke(json::value_to_tag<Gateway>, const json::value& jv);

} // namespace discord

} // namespace dc
