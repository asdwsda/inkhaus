#pragma once
// Alarm panel, lock and garage door widgets (task 108, docs/ui/widgets-new.md sections 5 and 6): view models and the
// action decisions. Pure logic, no drawing, no network. The keypad popup is in alarm_popup.h, the confirm dialog in
// confirm_dialog.h.
#include <string>

#include "config_model.h"
#include "entity_state.h"
#include "look.h"

namespace uistate {

enum class SecureIcon { Shield, ShieldOff, ShieldAlert, Lock, LockOpen, Alert, GarageOn, GarageOff, Warn };

// The 1x1 toggle-layout widget (name F24, icon M, state F24): text, icon and look.
struct SecureView {
  Look look = Look::Loading;
  std::string text = "-";
  SecureIcon icon = SecureIcon::ShieldOff;
  bool fill = false;        // needs attention: INK fill, PAPER content
  bool alertBadge = false;  // unsupported lock (needs a code): alert badge, the widget does nothing
};
// st: nullptr = nothing cached (loading). Text of a state the table does not know: humanised.
SecureView alarmView(const EntityState *st);
SecureView lockView(const EntityState *st);
SecureView garageView(const EntityState *st);
SecureView secureView(config::WidgetType type, const EntityState *st);

// ---- actions (section 6) ----
enum class SecureAction {
  None,     // nothing to do (loading, unavailable, locking / unlocking / jammed, unsupported lock)
  Lock,     // lock.lock
  Unlock,   // lock.unlock
  Open,     // cover.open_cover
  Close,    // cover.close_cover
  Stop,     // cover.stop_cover (opening / closing): no dialog
};
SecureAction lockAction(const EntityState *st);
SecureAction garageAction(const EntityState *st);
// Confirm dialog wanted? config `confirm` overrides; default: lock = unlock only, garage = both ways, stop never.
bool needsConfirm(config::WidgetType type, SecureAction a, const config::Widget &w);
// "Unlock Front door?", "Open Garage?", "Close Garage?" (the name is not truncated here: the renderer fits it).
std::string confirmTitle(SecureAction a, const std::string &name);

// ---- alarm panel details ----
enum class CodeFormat { None, Number, Text };
struct AlarmPanelInfo {
  CodeFormat codeFormat = CodeFormat::None;
  bool codeArmRequired = true;  // HA's default
  bool canHome = true, canAway = true, canNight = true;  // supported_features (unknown: all)
  int currentMode = -1;         // 0 disarmed, 1 home, 2 away, 3 night, -1 other (arming, pending, triggered, ...)
  bool unavailable = false;
  std::string stateLine = "-";  // popup state line: "Disarmed", "Armed home", ..., "TRIGGERED"
};
AlarmPanelInfo alarmPanelInfo(const EntityState *st);

}  // namespace uistate
