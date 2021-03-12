#include "gateway.hpp"

namespace dc {

namespace discord {

SessionStartLimit tag_invoke(json::value_to_tag<SessionStartLimit>, const json::value& jv) {
  SessionStartLimit s;
  const json::object& obj = jv.as_object();
  extract(obj, s.total, "total");
  extract(obj, s.remaining, "remaining");
  extract(obj, s.resetAfter, "reset_after");
  extract(obj, s.maxConcurrency, "max_concurrency");
  return s;
}

Gateway tag_invoke(json::value_to_tag<Gateway>, const json::value& jv) {
  Gateway g;
  const json::object& obj = jv.as_object();
  extract(obj, g.url, "url");
  extract(obj, g.shards, "shards");
  extract(obj, g.sessionStartLimit, "session_start_limit");
  return g;
}

} // namespace discord

} // namespace dc
