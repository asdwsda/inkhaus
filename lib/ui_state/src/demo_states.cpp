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

const char kDeg[] = "\xC2\xB0";

void attr(EntityState &e, const char *k, const char *v) { e.attrs.emplace_back(k, v); }

EntityState binary(const std::string &id, const char *cls, const char *state, const char *name = nullptr) {
  EntityState e = make(id, state, name);
  e.device_class = cls;
  return e;
}

EntityState cover(const std::string &id, const char *state, int pos) {
  EntityState e = make(id, state);
  e.current_position = pos;
  return e;
}

EntityState vacuum(const std::string &id, const char *state, int battery, const char *status = nullptr) {
  EntityState e = make(id, state);
  e.battery_level = battery;
  if (status) e.status = status;
  return e;
}

// Showcase states for the ids of demo.yaml; false for any other id.
bool showcase(const std::string &id, EntityState &out) {
  if (id == "sensor.living_room_climate") {
    out = make(id, "21.5");
    attr(out, "temperature", "21.54");
    attr(out, "temperature_unit", (std::string(kDeg) + "C").c_str());
    attr(out, "humidity", "45");
    attr(out, "humidity_unit", "%");
    attr(out, "dew_point", "9.3");
    attr(out, "dew_point_unit", (std::string(kDeg) + "C").c_str());
  } else if (id == "sensor.attic_climate") {
    out = make(id, "28.1");
    attr(out, "temperature", "28.1");
    attr(out, "temperature_unit", (std::string(kDeg) + "C").c_str());
    attr(out, "humidity", "31");
    attr(out, "humidity_unit", "%");
  } else if (id == "sensor.power_meter") {
    out = make(id, "1240");
    out.unit_of_measurement = "W";
    out.device_class = "power";
    attr(out, "today", "7.4");
    attr(out, "today_unit", "kWh");
  } else if (id == "sensor.pressure") {
    out = make(id, "1013");
    out.unit_of_measurement = "hPa";
    attr(out, "trend", "rising");
  } else if (id == "weather.garden") {
    out = make(id, "partlycloudy", "Garden");
    out.temperature = 17;
    out.humidity = 58;
    out.temperature_unit = std::string(kDeg) + "C";
  } else if (id == "binary_sensor.front_door") out = binary(id, "door", "on");
  else if (id == "binary_sensor.garage_door") out = binary(id, "garage_door", "off");
  else if (id == "binary_sensor.terrace_opening") out = binary(id, "opening", "on");
  else if (id == "binary_sensor.hall_motion") out = binary(id, "motion", "on");
  else if (id == "binary_sensor.office_occupancy") out = binary(id, "occupancy", "off");
  else if (id == "binary_sensor.phone_presence") out = binary(id, "presence", "on");
  else if (id == "binary_sensor.basement_moisture") out = binary(id, "moisture", "off");
  else if (id == "binary_sensor.kitchen_smoke") out = binary(id, "smoke", "off");
  else if (id == "binary_sensor.boiler_gas") out = binary(id, "gas", "on");
  else if (id == "binary_sensor.server_problem") out = binary(id, "problem", "on");
  else if (id == "binary_sensor.remote_battery") out = binary(id, "battery", "on");
  else if (id == "binary_sensor.router_connectivity") out = binary(id, "connectivity", "on");
  else if (id == "binary_sensor.front_lock") out = binary(id, "lock", "off");
  else if (id == "binary_sensor.charger_plug") out = binary(id, "plug", "on");
  else if (id == "binary_sensor.mains_power") out = binary(id, "power", "off");
  else if (id == "binary_sensor.plain_contact") out = make(id, "on");
  else if (id == "cover.terrace_shutter") out = cover(id, "open", 100);
  else if (id == "cover.hall_shutter") out = cover(id, "closing", 70);
  else if (id == "cover.bedroom_shutter") out = cover(id, "open", 25);
  else if (id == "cover.kitchen_shutter") out = cover(id, "closed", 0);
  else if (id == "vacuum.upstairs") out = vacuum(id, "cleaning", 64);
  else if (id == "vacuum.basement") out = vacuum(id, "returning", 15);
  else if (id == "vacuum.attic") out = vacuum(id, "error", 40, "Brush stuck");
  else if (id == "light.kitchen_ceiling" || id == "light.hall" || id == "light.office_desk") {
    out = make(id, "on");
    out.brightness = 128;
  } else if (id == "switch.dryer" || id == "switch.tv" || id == "switch.garden_pump") out = make(id, "on");
  else return false;
  return true;
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
  EntityState e;
  if (!showcase(id, e)) e = demoForBase(id);
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
