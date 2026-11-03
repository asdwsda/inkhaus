#include "notif_list.h"

#include <ArduinoJson.h>

#include <algorithm>

namespace {

std::string cut(const std::string& s, size_t max) {
  if (s.size() <= max) return s;
  size_t n = max;
  while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;  // do not split a UTF-8 sequence
  return s.substr(0, n);
}

bool is_space(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

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

std::string str_of(JsonVariantConst v) { return v.is<const char*>() ? v.as<const char*>() : ""; }

void add(std::vector<NotifItem>& out, JsonVariantConst n, const std::string& key) {
  if (!n.is<JsonObjectConst>()) return;
  NotifItem it;
  it.id = str_of(n["notification_id"]);
  if (it.id.empty()) it.id = key;
  if (it.id.empty()) return;  // cannot be dismissed, cannot be shown
  it.title = cut(str_of(n["title"]), kNotifTitleBytes);
  it.message = cut(notif_plain_line(str_of(n["message"])), kNotifMessageBytes);
  it.created = str_of(n["created_at"]);
  out.push_back(std::move(it));
}

}  // namespace

std::string notif_plain_line(const std::string& m) {
  std::string out;
  bool lineStart = true, space = false;
  for (size_t i = 0; i < m.size(); ++i) {
    const char c = m[i];
    if (c == '\n' || c == '\r') {
      lineStart = true;
      space = true;
      continue;
    }
    if (lineStart && (c == '#' || c == '>')) continue;  // heading / quote marks (the blank after them collapses)
    if (c == '[') {  // [text](url) keeps the text
      const size_t close = m.find("](", i);
      const size_t end = close == std::string::npos ? close : m.find(')', close);
      if (close != std::string::npos && end != std::string::npos && m.find('\n', i) > close) {
        if (space && !out.empty()) out += ' ';
        space = false;
        out += notif_plain_line(m.substr(i + 1, close - i - 1));
        i = end;
        lineStart = false;
        continue;
      }
    }
    if (c == '*' || c == '`') continue;
    if (c == '_' && i + 1 < m.size() && m[i + 1] == '_') {
      ++i;
      continue;
    }
    if (c == ' ' || c == '\t') {
      space = true;
      continue;
    }
    if (space && !out.empty()) out += ' ';
    space = false;
    lineStart = false;
    out += c;
  }
  return out;
}

NotifList parse_notification_list(const std::string& json) {
  NotifList out;
  const char shape = result_shape(json);
  if (shape != '[' && shape != '{') return out;
  JsonDocument filter;
  if (shape == '[') {
    for (const char* k : {"notification_id", "created_at", "title", "message"}) filter["result"][0][k] = true;
  } else {
    for (const char* k : {"notification_id", "created_at", "title", "message"}) filter["result"]["*"][k] = true;
  }
  filter["success"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, json, DeserializationOption::Filter(filter))) return out;
  if (doc["success"].is<bool>() && !doc["success"].as<bool>()) return out;
  JsonVariantConst r = doc["result"];
  if (r.is<JsonArrayConst>()) {
    for (JsonVariantConst v : r.as<JsonArrayConst>()) add(out.items, v, "");
  } else if (r.is<JsonObjectConst>()) {
    for (JsonPairConst kv : r.as<JsonObjectConst>()) add(out.items, kv.value(), kv.key().c_str());
  } else {
    return out;
  }
  out.ok = true;
  std::stable_sort(out.items.begin(), out.items.end(),
                   [](const NotifItem& a, const NotifItem& b) { return a.created > b.created; });
  if (out.items.size() > kNotifMaxItems) out.items.resize(kNotifMaxItems);
  return out;
}

std::string build_notification_dismiss_command(int id, const std::string& notificationId) {
  JsonDocument doc;
  doc["id"] = id;
  doc["type"] = "persistent_notification/dismiss";
  doc["notification_id"] = notificationId;
  std::string s;
  serializeJson(doc, s);
  return s;
}
