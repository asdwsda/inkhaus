#include "state_parser.h"

#include <ArduinoJson.h>

#include <algorithm>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

std::string str_of(JsonVariantConst v) {
  return v.is<const char*>() ? std::string(v.as<const char*>()) : std::string();
}
// Finite doubles only; optionally range-limited to [lo, hi].
std::optional<double> dbl_of(JsonVariantConst v, double lo = -std::numeric_limits<double>::max(),
                             double hi = std::numeric_limits<double>::max()) {
  if (!v.is<double>() || v.isNull()) return std::nullopt;
  double d = v.as<double>();
  if (!std::isfinite(d) || d < lo || d > hi) return std::nullopt;
  return d;
}
// Integer in [lo, hi]; out-of-range is absent. Range check happens before the cast.
std::optional<int> int_of(JsonVariantConst v, int lo, int hi) {
  std::optional<double> d = dbl_of(v, lo, hi);
  if (!d) return std::nullopt;
  return (int)*d;
}

// last_changed epoch seconds: must be >= 0 and fit int64 (2^63 is not representable).
int64_t lc_of(JsonVariantConst v) {
  std::optional<double> d = dbl_of(v, 0.0, 9223372036854774784.0);
  return d ? (int64_t)*d : 0;
}

constexpr size_t kMaxAttrs = 16;  // scalar attributes kept per entity

// Fills the typed fields of `e` from the attribute object `a` (same rules for both parsers).
void fill_attributes(EntityState& e, JsonVariantConst a) {
    e.friendly_name = str_of(a["friendly_name"]);
    e.current_position = int_of(a["current_position"], 0, 100);
    e.supported_features = int_of(a["supported_features"], 0, 1 << 30);
    e.brightness = int_of(a["brightness"], 0, 255);
    if (a["supported_color_modes"].is<JsonArrayConst>()) {
      for (JsonVariantConst m : a["supported_color_modes"].as<JsonArrayConst>()) {
        if (!m.is<const char*>()) continue;
        if (!e.color_modes.empty()) e.color_modes += ',';
        e.color_modes += m.as<const char*>();
      }
    }
    e.current_temperature = dbl_of(a["current_temperature"]);
    e.temperature = dbl_of(a["temperature"]);
    e.current_humidity = dbl_of(a["current_humidity"], 0, 100);
    e.humidity = dbl_of(a["humidity"], 0, 100);
    e.hvac_mode = str_of(a["hvac_mode"]);
    e.hvac_action = str_of(a["hvac_action"]);
    e.min_temp = dbl_of(a["min_temp"]);
    e.max_temp = dbl_of(a["max_temp"]);
    e.target_temp_step = dbl_of(a["target_temp_step"]);
    e.unit_of_measurement = str_of(a["unit_of_measurement"]);
    e.device_class = str_of(a["device_class"]);
    e.temperature_unit = str_of(a["temperature_unit"]);
    e.battery_level = int_of(a["battery_level"], 0, 100);
    e.status = str_of(a["status"]);
    if (a.is<JsonObjectConst>()) {
      for (JsonPairConst kv : a.as<JsonObjectConst>()) {
        if (e.attrs.size() >= kMaxAttrs) break;
        JsonVariantConst val = kv.value();
        std::string text;
        if (val.is<const char*>()) {
          text = val.as<const char*>();
        } else if (val.is<bool>()) {
          text = val.as<bool>() ? "true" : "false";
        } else if (val.is<double>() && !val.isNull()) {
          if (!dbl_of(val)) continue;  // not finite
          serializeJson(val, text);
        } else {
          continue;  // null, nested object/array
        }
        e.attrs.emplace_back(kv.key().c_str(), text);
      }
    }
}

// Seconds since the epoch of "2024-05-01T10:20:30.123456+00:00" / "...Z"; 0 if unparsable.
int64_t iso_to_epoch(const char* t) {
  int y, mo, d, h, mi, sec;
  if (!t || sscanf(t, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &sec) != 6) return 0;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || y < 1970) return 0;
  // days from civil (Howard Hinnant)
  y -= mo <= 2;
  const int era = y / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = (int64_t)era * 146097 + doe - 719468;
  int64_t epoch = days * 86400 + h * 3600 + mi * 60 + sec;
  const char* z = t + 19;
  if (*z == '.') while (*++z >= '0' && *z <= '9') {}
  if ((*z == '+' || *z == '-') && strlen(z) >= 6) {
    int oh = 0, om = 0;
    if (sscanf(z + 1, "%2d:%2d", &oh, &om) == 2) {
      int64_t off = oh * 3600 + om * 60;
      epoch -= (*z == '+') ? off : -off;
    }
  }
  return epoch < 0 ? 0 : epoch;
}

}  // namespace

ParseResult parse_state_response(const std::string& json,
                                 const std::vector<std::string>& requested_ids) {
  ParseResult r;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    r.error = std::string("bad JSON: ") + err.c_str();
    return r;
  }
  if (!doc.is<JsonObjectConst>()) {
    r.error = "response is not a JSON object";
    return r;
  }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  for (const std::string& id : requested_ids) {
    EntityState e;
    e.id = id;
    JsonVariantConst v = root[id];
    JsonVariantConst s = v["s"];
    if (v.is<JsonObjectConst>() && s.is<const char*>()) {
      e.state = s.as<const char*>();
      e.available = e.state != "unavailable";
      e.last_changed = lc_of(v["lc"]);
      JsonVariantConst a = v["a"];
      fill_attributes(e, a);
    }
    r.states.push_back(e);
  }
  r.ok = true;
  return r;
}

ParseResult parse_service_response(const std::string& json,
                                   const std::vector<std::string>& wanted_ids,
                                   const ExtraAttrs& extra) {
  ParseResult r;
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    r.error = "bad JSON";
    return r;
  }
  if (!doc.is<JsonArrayConst>()) {
    r.error = "response is not a JSON array";
    return r;
  }
  for (JsonVariantConst item : doc.as<JsonArrayConst>()) {
    if (!item.is<JsonObjectConst>() || !item["entity_id"].is<const char*>() ||
        !item["state"].is<const char*>())
      continue;
    const std::string id = item["entity_id"].as<const char*>();
    if (std::find(wanted_ids.begin(), wanted_ids.end(), id) == wanted_ids.end()) continue;
    bool dup = false;
    for (const EntityState& s : r.states) dup |= s.id == id;
    if (dup) continue;
    EntityState e;
    e.id = id;
    e.state = item["state"].as<const char*>();
    e.available = e.state != "unavailable";
    e.last_changed = iso_to_epoch(item["last_changed"].as<const char*>());
    // Only the attributes the template path would have fetched, in the same order.
    JsonDocument filtered;
    JsonVariantConst attrs = item["attributes"];
    for (const std::string& name : state_attribute_names(id, extra)) {
      JsonVariantConst v = attrs[name];
      if (!v.isNull()) filtered[name] = v;
    }
    fill_attributes(e, filtered.as<JsonVariantConst>());
    r.states.push_back(std::move(e));
  }
  r.ok = true;
  return r;
}
