#include "secure_view.h"

#include "widget_view.h"

namespace uistate {

namespace {

SecureView unavailable() {
  SecureView v;
  v.look = Look::Unavailable;
  v.text = "n/a";
  v.icon = SecureIcon::Warn;
  return v;
}

SecureView make(const char *text, SecureIcon icon, bool fill = false) {
  SecureView v;
  v.look = Look::Normal;
  v.text = text;
  v.icon = icon;
  v.fill = fill;
  return v;
}

const std::string *codeFormatAttr(const EntityState &st) { return st.attr("code_format"); }

}  // namespace

SecureView alarmView(const EntityState *st) {
  if (!st) {
    SecureView v;
    v.icon = SecureIcon::ShieldOff;
    return v;
  }
  if (!st->available) return unavailable();
  const std::string &s = st->state;
  if (s == "disarmed") return make("Off", SecureIcon::ShieldOff);
  if (s == "armed_home") return make("Home", SecureIcon::Shield);
  if (s == "armed_away") return make("Away", SecureIcon::Shield);
  if (s == "armed_night") return make("Night", SecureIcon::Shield);
  if (s == "armed_vacation") return make("Vacat.", SecureIcon::Shield);
  if (s == "armed_custom_bypass") return make("Custom", SecureIcon::Shield);
  if (s == "arming") return make("Arming", SecureIcon::ShieldOff);
  if (s == "pending") return make("Entry", SecureIcon::ShieldOff);
  if (s == "triggered") return make("ALARM", SecureIcon::ShieldAlert, true);
  SecureView v = make("", SecureIcon::ShieldOff);
  v.text = humanise(s);
  return v;
}

SecureView lockView(const EntityState *st) {
  if (!st) {
    SecureView v;
    v.icon = SecureIcon::Lock;
    return v;
  }
  if (!st->available) return unavailable();
  const std::string &s = st->state;
  SecureView v;
  if (s == "locked") v = make("Locked", SecureIcon::Lock);
  else if (s == "unlocked" || s == "open") v = make("Open", SecureIcon::LockOpen, true);
  else if (s == "locking") v = make("Locking", SecureIcon::Lock);
  else if (s == "unlocking") v = make("Opening", SecureIcon::LockOpen);
  else if (s == "jammed") v = make("Jammed", SecureIcon::Alert, true);
  else {
    v = make("", SecureIcon::Lock);
    v.text = humanise(s);
  }
  const std::string *cf = codeFormatAttr(*st);
  v.alertBadge = cf && !cf->empty();  // v1: locks that need a code are not supported
  return v;
}

SecureView garageView(const EntityState *st) {
  if (!st) {
    SecureView v;
    v.icon = SecureIcon::GarageOff;
    return v;
  }
  if (!st->available) return unavailable();
  const std::string &s = st->state;
  if (s == "closed") return make("Closed", SecureIcon::GarageOff);
  if (s == "open") return make("Open", SecureIcon::GarageOn, true);
  if (s == "opening") return make("Opening", SecureIcon::GarageOn);
  if (s == "closing") return make("Closing", SecureIcon::GarageOff);
  SecureView v = make("", SecureIcon::GarageOff);
  v.text = humanise(s);
  return v;
}

SecureView secureView(config::WidgetType type, const EntityState *st) {
  switch (type) {
    case config::WidgetType::Alarm: return alarmView(st);
    case config::WidgetType::Lock: return lockView(st);
    default: return garageView(st);
  }
}

SecureAction lockAction(const EntityState *st) {
  if (!st || !st->available) return SecureAction::None;
  const std::string *cf = codeFormatAttr(*st);
  if (cf && !cf->empty()) return SecureAction::None;  // needs a code: unsupported
  if (st->state == "locked") return SecureAction::Unlock;
  if (st->state == "unlocked" || st->state == "open") return SecureAction::Lock;
  return SecureAction::None;  // locking / unlocking / jammed / unknown
}

SecureAction garageAction(const EntityState *st) {
  if (!st || !st->available) return SecureAction::None;
  if (st->state == "closed") return SecureAction::Open;
  if (st->state == "open") return SecureAction::Close;
  if (st->state == "opening" || st->state == "closing") return SecureAction::Stop;
  return SecureAction::None;
}

bool needsConfirm(config::WidgetType type, SecureAction a, const config::Widget &w) {
  if (a == SecureAction::None || a == SecureAction::Stop) return false;
  if (w.confirm) return *w.confirm;
  if (type == config::WidgetType::Lock) return a == SecureAction::Unlock;
  return true;  // garage: both ways
}

std::string confirmTitle(SecureAction a, const std::string &name) {
  const char *verb = a == SecureAction::Unlock ? "Unlock" : a == SecureAction::Lock ? "Lock"
                     : a == SecureAction::Open ? "Open" : "Close";
  return std::string(verb) + " " + name + "?";
}

AlarmPanelInfo alarmPanelInfo(const EntityState *st) {
  AlarmPanelInfo i;
  if (!st) return i;
  if (!st->available) {
    i.unavailable = true;
    i.stateLine = "n/a";
    return i;
  }
  if (const std::string *cf = st->attr("code_format")) {
    if (*cf == "number") i.codeFormat = CodeFormat::Number;
    else if (*cf == "text") i.codeFormat = CodeFormat::Text;
  }
  if (const std::string *r = st->attr("code_arm_required")) i.codeArmRequired = *r != "false";
  if (st->supported_features) {
    i.canHome = (*st->supported_features & 1) != 0;
    i.canAway = (*st->supported_features & 2) != 0;
    i.canNight = (*st->supported_features & 4) != 0;
  }
  const std::string &s = st->state;
  if (s == "disarmed") { i.currentMode = 0; i.stateLine = "Disarmed"; }
  else if (s == "armed_home") { i.currentMode = 1; i.stateLine = "Armed home"; }
  else if (s == "armed_away") { i.currentMode = 2; i.stateLine = "Armed away"; }
  else if (s == "armed_night") { i.currentMode = 3; i.stateLine = "Armed night"; }
  else if (s == "armed_vacation") i.stateLine = "Armed vacation";
  else if (s == "armed_custom_bypass") i.stateLine = "Armed custom";
  else if (s == "arming") i.stateLine = "Arming";
  else if (s == "pending") i.stateLine = "Pending";
  else if (s == "triggered") i.stateLine = "TRIGGERED";
  else i.stateLine = humanise(s);
  return i;
}

}  // namespace uistate
