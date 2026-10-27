#pragma once
// Typed snapshot of one Home Assistant entity. Pure C++, no Arduino includes.
#include <cstdint>
#include <optional>
#include <string>

struct EntityState {
  std::string id;
  std::string state;       // raw HA state string ("on", "open", "21.5", ...)
  bool available = false;  // false for unavailable / unknown / missing entities
  int64_t last_changed = 0;  // epoch seconds, 0 if unknown
  std::string friendly_name;

  // cover
  std::optional<int> current_position;
  // light
  std::optional<int> brightness;
  // climate (temperature / humidity are shared with weather)
  std::optional<double> current_temperature;
  std::optional<double> temperature;
  std::optional<double> current_humidity;
  std::optional<double> humidity;
  std::string hvac_mode;
  std::string hvac_action;
  std::optional<double> min_temp;
  std::optional<double> max_temp;
  std::optional<double> target_temp_step;
  // sensor
  std::string unit_of_measurement;
  std::string device_class;
  // weather
  std::string temperature_unit;
  // vacuum
  std::optional<int> battery_level;
  std::string status;

  bool operator==(const EntityState& o) const {
    return id == o.id && state == o.state && available == o.available &&
           last_changed == o.last_changed && friendly_name == o.friendly_name &&
           current_position == o.current_position && brightness == o.brightness &&
           current_temperature == o.current_temperature && temperature == o.temperature &&
           current_humidity == o.current_humidity && humidity == o.humidity &&
           hvac_mode == o.hvac_mode && hvac_action == o.hvac_action &&
           min_temp == o.min_temp && max_temp == o.max_temp &&
           target_temp_step == o.target_temp_step &&
           unit_of_measurement == o.unit_of_measurement && device_class == o.device_class &&
           temperature_unit == o.temperature_unit && battery_level == o.battery_level &&
           status == o.status;
  }
  bool operator!=(const EntityState& o) const { return !(*this == o); }
};
