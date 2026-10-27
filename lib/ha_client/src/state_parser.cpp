#include "state_parser.h"

#include <ArduinoJson.h>

#include <cmath>
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
      e.available = e.state != "unavailable" && e.state != "unknown";
      e.last_changed = lc_of(v["lc"]);
      JsonVariantConst a = v["a"];
      e.friendly_name = str_of(a["friendly_name"]);
      e.current_position = int_of(a["current_position"], 0, 100);
      e.brightness = int_of(a["brightness"], 0, 255);
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
    }
    r.states.push_back(e);
  }
  r.ok = true;
  return r;
}
