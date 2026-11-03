#include "notifications.h"

#include <ArduinoJson.h>

namespace {

bool is_space(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

// First non-space character after the `"result":` key of the message ('[' or '{'), or 0. The text `"result"`
// also appears as the value of `"type"`; only an occurrence followed by a colon is the key.
char result_shape(const std::string& json) {
  size_t pos = 0;
  while ((pos = json.find("\"result\"", pos)) != std::string::npos) {
    pos += 8;
    size_t q = pos;
    while (q < json.size() && is_space(json[q])) ++q;
    if (q >= json.size() || json[q] != ':') continue;
    for (++q; q < json.size(); ++q)
      if (!is_space(json[q])) return json[q];
    return 0;
  }
  return 0;
}

std::string first_line(const std::string& s) {
  const size_t nl = s.find_first_of("\r\n");
  return nl == std::string::npos ? s : s.substr(0, nl);
}

std::string cut(const std::string& s, size_t max) {
  if (s.size() <= max) return s;
  size_t n = max;
  while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;  // do not split a UTF-8 sequence
  return s.substr(0, n);
}

struct Newest {
  int count = 0;
  std::string created, title;
  void add(JsonVariantConst n) {
    if (!n.is<JsonObjectConst>()) return;
    ++count;
    const char* c = n["created_at"].is<const char*>() ? n["created_at"].as<const char*>() : "";
    if (count > 1 && created.compare(c) >= 0) return;
    created = c;
    std::string t = n["title"].is<const char*>() ? n["title"].as<const char*>() : "";
    if (t.empty() && n["message"].is<const char*>()) t = first_line(n["message"].as<const char*>());
    title = t;
  }
};

}  // namespace

std::string build_notifications_command(int id) {
  return "{\"id\":" + std::to_string(id) + ",\"type\":\"persistent_notification/get\"}";
}

NotificationSummary parse_notifications(const std::string& json) {
  NotificationSummary out;
  const char shape = result_shape(json);
  if (shape != '[' && shape != '{') return out;
  JsonDocument filter;
  if (shape == '[') {
    filter["result"][0]["created_at"] = true;
    filter["result"][0]["title"] = true;
    filter["result"][0]["message"] = true;
  } else {
    filter["result"]["*"]["created_at"] = true;
    filter["result"]["*"]["title"] = true;
    filter["result"]["*"]["message"] = true;
  }
  filter["success"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, json, DeserializationOption::Filter(filter))) return out;
  if (doc["success"].is<bool>() && !doc["success"].as<bool>()) return out;
  Newest n;
  JsonVariantConst r = doc["result"];
  if (r.is<JsonArrayConst>()) {
    for (JsonVariantConst v : r.as<JsonArrayConst>()) n.add(v);
  } else if (r.is<JsonObjectConst>()) {
    for (JsonPairConst kv : r.as<JsonObjectConst>()) n.add(kv.value());
  } else {
    return out;
  }
  out.ok = true;
  out.count = n.count;
  out.newest = cut(n.title, 120);
  return out;
}
