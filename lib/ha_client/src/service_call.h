#pragma once
#include <string>
#include <vector>

enum class Action {
  CoverOpen,
  CoverClose,
  CoverStop,
  LightToggle,   // kept for compatibility; the UI sends TurnOn/TurnOff
  SwitchToggle,  // kept for compatibility; the UI sends TurnOn/TurnOff
  LightTurnOn,
  LightTurnOff,
  SwitchTurnOn,
  SwitchTurnOff,
  AutomationTurnOn,
  AutomationTurnOff,
  VacuumStart,
  VacuumPause,
  VacuumReturnToBase,
  MediaPlayPause,  // task 106: media_player.*
  MediaPrevious,
  MediaNext,
  MediaTurnOn,
  LockLock,  // task 108: lock.*
  LockUnlock,
};

struct ServiceCall {
  std::string domain;
  std::string service;
  std::string body;  // JSON, entity_id is always a list
};

// Builds the call for a parameterless action. Returns false if ids is empty.
bool build_service_call(Action action, const std::vector<std::string>& ids, ServiceCall& out);

// Rounds `target` to the nearest multiple of `step` (step <= 0: no rounding)
// and clamps it to [min_temp, max_temp] (ignored if min_temp > max_temp).
double normalize_target_temperature(double target, double min_temp, double max_temp, double step);

// climate.set_temperature with the normalized target. Returns false if ids is empty.
bool build_climate_set_temperature(const std::vector<std::string>& ids, double target,
                                   double min_temp, double max_temp, double step,
                                   ServiceCall& out);

// media_player.volume_set with volume_level = pct / 100 (pct clamped to 0..100). Returns false if ids is empty.
bool build_media_volume_set(const std::vector<std::string>& ids, int pct, ServiceCall& out);
// media_player.volume_mute with is_volume_muted. Returns false if ids is empty.
bool build_media_volume_mute(const std::vector<std::string>& ids, bool mute, ServiceCall& out);

// Light brightness in percent: pct <= 0 -> light.turn_off, otherwise light.turn_on with
// brightness_pct (clamped to 1..100). Returns false if ids is empty.
bool build_light_brightness(const std::vector<std::string>& ids, int pct, ServiceCall& out);

// Alarm panel commands (task 108): alarm_control_panel.alarm_disarm / alarm_arm_home / alarm_arm_away / alarm_arm_night.
enum class AlarmCmd { Disarm, ArmHome, ArmAway, ArmNight };
// `code` goes into the body as "code" when not empty (digits only; callers never log the body). Returns false if ids is empty.
bool build_alarm_call(const std::vector<std::string>& ids, AlarmCmd cmd, const std::string& code, ServiceCall& out);
