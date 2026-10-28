#include "demo_states.h"

namespace uistate {

std::string prettyName(const std::string &id) {
  std::string s = id.substr(id.find('.') == std::string::npos ? 0 : id.find('.') + 1);
  for (char &c : s) if (c == '_') c = ' ';
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = char(s[0] - 'a' + 'A');
  return s;
}

namespace {

EntityState make(const std::string &id, const char *state, const char *name = nullptr) {
  EntityState e;
  e.id = id;
  e.state = state;
  e.available = true;
  e.friendly_name = name ? name : prettyName(id);
  return e;
}

EntityState demoForBase(const std::string &id) {
  const std::string domain = id.substr(0, id.find('.'));
  if (domain == "cover") {
    if (id == "cover.living_room_shutter") {
      EntityState e = make(id, "opening", "Living room shutter");
      e.current_position = 40;
      return e;
    }
    EntityState e = make(id, "closed");
    e.current_position = 0;
    return e;
  }
  if (domain == "light") {
    if (id == "light.living_room_ceiling") {
      EntityState e = make(id, "on", "Ceiling");
      e.brightness = 204;  // 80 %
      return e;
    }
    if (id == "light.living_room_floor_lamp") return make(id, "off", "Floor lamp");
    return make(id, "off");
  }
  if (domain == "switch") return make(id, "off", id == "switch.living_room_fan" ? "Fan" : nullptr);
  if (domain == "automation") return make(id, "on", id == "automation.night_mode" ? "Night mode" : nullptr);
  if (domain == "binary_sensor") {
    const bool window = id == "binary_sensor.living_room_window";
    EntityState e = make(id, "on", window ? "Window" : nullptr);
    if (window) e.device_class = "window";
    return e;
  }
  if (domain == "climate") {
    const bool living = id == "climate.living_room";
    EntityState e = make(id, "heat", living ? "Living room" : nullptr);
    e.hvac_mode = "heat";
    e.hvac_action = living ? "heating" : "idle";
    e.current_temperature = living ? 21.5 : 19.0;
    e.temperature = living ? 22.0 : 19.5;
    e.current_humidity = living ? 45 : 52;
    e.min_temp = 5;
    e.max_temp = 30;
    e.target_temp_step = 0.5;
    return e;
  }
  if (domain == "sensor") {
    if (id == "sensor.outside_temperature") {
      EntityState e = make(id, "8.4", "Outside temperature");
      e.unit_of_measurement = "\xC2\xB0" "C";
      e.device_class = "temperature";
      return e;
    }
    return make(id, "home");
  }
  if (domain == "weather") {
    EntityState e = make(id, "cloudy", "Home");
    e.temperature = 12;
    e.humidity = 76;
    e.temperature_unit = "\xC2\xB0" "C";
    return e;
  }
  if (domain == "vacuum") {
    EntityState e = make(id, "docked", "Robot");
    e.battery_level = 85;
    return e;
  }
  return make(id, "home");
}

// Ids ending in "_unavailable" exist to show the unavailable look (test configs).
EntityState demoFor(const std::string &id) {
  EntityState e = demoForBase(id);
  const std::string suffix = "_unavailable";
  if (id.size() > suffix.size() && id.compare(id.size() - suffix.size(), suffix.size(), suffix) == 0) {
    e.state = "unavailable";
    e.available = false;
  }
  return e;
}

}  // namespace

std::vector<EntityState> demoStates(const config::Config &cfg) {
  std::vector<EntityState> out;
  for (const std::string &id : cfg.entityIds) out.push_back(demoFor(id));
  return out;
}

}  // namespace uistate
