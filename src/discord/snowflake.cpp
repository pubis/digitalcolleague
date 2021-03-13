#include "snowflake.hpp"

namespace dc {

namespace discord {

Snowflake tag_invoke(json::value_to_tag<Snowflake>, const json::value& jv) {
  auto s = json::value_to<std::string>(jv);
  return { std::stoull(s) };
}

} // namespace discord

} // namespace dc
