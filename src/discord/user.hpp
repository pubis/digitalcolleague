#pragma once

#include "../common.hpp"
#include "snowflake.hpp"

namespace dc {

namespace discord {

struct User {
  Snowflake id;
  std::string username;
  std::string discriminator;
  std::optional<std::string> avatar;
  bool bot;
};

User tag_invoke(json::value_to_tag<User>, const json::value& jv);

} // namespace discord

} // namespace dc
