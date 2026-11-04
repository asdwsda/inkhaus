#include "service_call.h"

#include <ArduinoJson.h>

#include <cmath>

namespace {

std::string body_for(const std::vector<std::string>& ids, const double* temperature,
                     const int* brightness_pct = nullptr) {
  JsonDocument doc;
  JsonArray arr = doc["entity_id"].to<JsonArray>();
  for (const std::string& id : ids) arr.add(id);
  if (temperature) doc["temperature"] = *temperature;
  if (brightness_pct) doc["brightness_pct"] = *brightness_pct;
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace

bool build_service_call(Action action, const std::vector<std::string>& ids, ServiceCall& out) {
  if (ids.empty()) return false;
  switch (action) {
    case Action::CoverOpen: out.domain = "cover"; out.service = "open_cover"; break;
    case Action::CoverClose: out.domain = "cover"; out.service = "close_cover"; break;
    case Action::CoverStop: out.domain = "cover"; out.service = "stop_cover"; break;
    case Action::LightToggle: out.domain = "light"; out.service = "toggle"; break;
    case Action::SwitchToggle: out.domain = "switch"; out.service = "toggle"; break;
    case Action::LightTurnOn: out.domain = "light"; out.service = "turn_on"; break;
    case Action::LightTurnOff: out.domain = "light"; out.service = "turn_off"; break;
    case Action::SwitchTurnOn: out.domain = "switch"; out.service = "turn_on"; break;
    case Action::SwitchTurnOff: out.domain = "switch"; out.service = "turn_off"; break;
    case Action::AutomationTurnOn: out.domain = "automation"; out.service = "turn_on"; break;
    case Action::AutomationTurnOff: out.domain = "automation"; out.service = "turn_off"; break;
    case Action::VacuumStart: out.domain = "vacuum"; out.service = "start"; break;
    case Action::VacuumPause: out.domain = "vacuum"; out.service = "pause"; break;
    case Action::VacuumReturnToBase: out.domain = "vacuum"; out.service = "return_to_base"; break;
    case Action::MediaPlayPause: out.domain = "media_player"; out.service = "media_play_pause"; break;
    case Action::MediaPrevious: out.domain = "media_player"; out.service = "media_previous_track"; break;
    case Action::MediaNext: out.domain = "media_player"; out.service = "media_next_track"; break;
    case Action::MediaTurnOn: out.domain = "media_player"; out.service = "turn_on"; break;
    case Action::LockLock: out.domain = "lock"; out.service = "lock"; break;
    case Action::LockUnlock: out.domain = "lock"; out.service = "unlock"; break;
    default: return false;
  }
  out.body = body_for(ids, nullptr);
  return true;
}

double normalize_target_temperature(double target, double min_temp, double max_temp, double step) {
  bool clamp = min_temp <= max_temp;
  if (clamp) target = std::fmin(std::fmax(target, min_temp), max_temp);
  if (step > 0) {
    target = std::round(target / step) * step;
    target = std::round(target * 100.0) / 100.0;  // drop float noise
    if (clamp) target = std::fmin(std::fmax(target, min_temp), max_temp);
  }
  return target;
}

bool build_climate_set_temperature(const std::vector<std::string>& ids, double target,
                                   double min_temp, double max_temp, double step,
                                   ServiceCall& out) {
  if (ids.empty()) return false;
  double t = normalize_target_temperature(target, min_temp, max_temp, step);
  out.domain = "climate";
  out.service = "set_temperature";
  out.body = body_for(ids, &t);
  return true;
}

bool build_light_brightness(const std::vector<std::string>& ids, int pct, ServiceCall& out) {
  if (ids.empty()) return false;
  out.domain = "light";
  if (pct <= 0) {
    out.service = "turn_off";
    out.body = body_for(ids, nullptr);
  } else {
    int p = pct > 100 ? 100 : pct;
    out.service = "turn_on";
    out.body = body_for(ids, nullptr, &p);
  }
  return true;
}

bool build_media_volume_set(const std::vector<std::string>& ids, int pct, ServiceCall& out) {
  if (ids.empty()) return false;
  const int p = pct < 0 ? 0 : pct > 100 ? 100 : pct;
  JsonDocument doc;
  JsonArray arr = doc["entity_id"].to<JsonArray>();
  for (const std::string& id : ids) arr.add(id);
  doc["volume_level"] = p / 100.0;
  out.domain = "media_player";
  out.service = "volume_set";
  serializeJson(doc, out.body);
  return true;
}

bool build_media_volume_mute(const std::vector<std::string>& ids, bool mute, ServiceCall& out) {
  if (ids.empty()) return false;
  JsonDocument doc;
  JsonArray arr = doc["entity_id"].to<JsonArray>();
  for (const std::string& id : ids) arr.add(id);
  doc["is_volume_muted"] = mute;
  out.domain = "media_player";
  out.service = "volume_mute";
  serializeJson(doc, out.body);
  return true;
}

bool build_alarm_call(const std::vector<std::string>& ids, AlarmCmd cmd, const std::string& code, ServiceCall& out) {
  if (ids.empty()) return false;
  out.domain = "alarm_control_panel";
  switch (cmd) {
    case AlarmCmd::Disarm: out.service = "alarm_disarm"; break;
    case AlarmCmd::ArmHome: out.service = "alarm_arm_home"; break;
    case AlarmCmd::ArmAway: out.service = "alarm_arm_away"; break;
    case AlarmCmd::ArmNight: out.service = "alarm_arm_night"; break;
  }
  JsonDocument doc;
  JsonArray arr = doc["entity_id"].to<JsonArray>();
  for (const std::string& id : ids) arr.add(id);
  if (!code.empty()) doc["code"] = code;
  out.body.clear();
  serializeJson(doc, out.body);
  return true;
}
