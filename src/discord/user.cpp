#include "user.hpp"

namespace dc {

namespace discord {

User tag_invoke(json::value_to_tag<User>, const json::value& jv) {
  User u;
  auto object = jv.as_object();
  extract(object, u.id, "id");
  extract(object, u.username, "username");
  extract(object, u.discriminator, "discriminator");
  extract_optional(object, u.avatar, "avatar");
  extract_maybe(object, u.bot, "bot", false);
  return u;
}

} // namespace discord

} // namespace dc
