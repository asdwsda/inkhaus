// AppState: alarm keypad and the lock / garage confirm dialog (task 108, docs/ui/widgets-new.md 5-7).
// Kept apart from app_state.cpp. The code typed on the keypad only travels in an AlarmResult to the sink; it is never
// stored in the AppState and never logged.
#include "app_state.h"

namespace uistate {

using config::WidgetType;

namespace {

void setStateOf(EntityState &s, AlarmCmd cmd) {
  s.state = cmd == AlarmCmd::Disarm ? "disarmed" : cmd == AlarmCmd::ArmHome ? "armed_home"
            : cmd == AlarmCmd::ArmAway ? "armed_away" : "armed_night";
}

}  // namespace

AlarmPanelInfo AppState::alarmInfo() const { return alarmPanelInfo(store_.get(alarm_.entityId())); }

// A tap / rocker push on an alarm, lock or garage widget.
bool AppState::activateSecure(int idx, uint32_t now, bool viaRocker) {
  if (screen_ != Screen::Tabs || modalOpen()) return false;
  const config::Widget *w = widgetAt(shownTab_, idx);
  if (!w || w->entities.empty()) return false;
  const WidgetData d = widgetData(idx);  // members with the optimistic state of a pending action
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  if (w->type == WidgetType::Alarm) {
    if (!st || !st->available) return false;  // loading / unavailable: nothing to open
    aAwaiting_ = aSawPending_ = false;
    alarm_.open(w->entities[0], shownTab_, idx, viaRocker, now);
    return true;
  }
  const SecureAction act = w->type == WidgetType::Lock ? lockAction(st) : garageAction(st);
  if (act == SecureAction::None) return false;  // locking / unlocking / jammed / unsupported / loading
  const int sub = act == SecureAction::Stop ? 2 : 0;
  if (needsConfirm(w->type, act, *w)) {
    confirm_.open(confirmTitle(act, widgetTitle(idx)), idx, sub, now);
    confirm_.bind(shownTab_, w->entities[0], act);
    return true;
  }
  return runAction(idx, sub, now);
}

bool AppState::confirmAnswer(ConfirmAnswer a, uint32_t now) {
  if (a == ConfirmAnswer::None) return false;
  const int idx = confirm_.widget(), sub = confirm_.sub();
  if (a == ConfirmAnswer::Yes) {
    // Bound to what was asked: same tab, same entity, same action as live state offers now. Otherwise do nothing.
    const config::Widget *w = widgetAt(shownTab_, idx);
    bool ok = shownTab_ == confirm_.tab() && w && !w->entities.empty() && w->entities[0] == confirm_.entity();
    if (ok) {
      const WidgetData d = widgetData(idx);
      const EntityState *st = d.members.empty() ? nullptr : d.members[0];
      const SecureAction live = w->type == WidgetType::Lock ? lockAction(st)
                                : w->type == WidgetType::Garage ? garageAction(st) : SecureAction::None;
      ok = live == confirm_.action();
    }
    if (ok) runAction(idx, sub, now);
  }
  return true;  // the dialog closed (and the widget may show the pending result)
}

bool AppState::closeAlarm() {
  const bool was = alarm_.isOpen();
  alarm_.close();  // the code is cleared
  aAwaiting_ = aSawPending_ = false;
  return was;
}

bool AppState::takeKeypadHint(ui::Rect &r) {
  const bool had = keyHint_;
  keyHint_ = false;
  r = keyHintRect_;
  return had && alarm_.isOpen();
}

bool AppState::alarmResult(const AlarmResult &r, uint32_t now) {
  keyHint_ = r.keyOnly && r.changed;
  keyHintRect_ = r.dirty;
  if (r.closed) {
    aAwaiting_ = aSawPending_ = false;
    return true;
  }
  if (!r.send) return r.changed;
  const EntityState *st = store_.get(alarm_.entityId());
  aStateAtSend_ = st ? st->state : std::string();
  aCodeSent_ = !r.code.empty();
  aAwaiting_ = true;
  aSawPending_ = false;
  aSentAt_ = now;
  if (marksDriven_) {
    if (asink_) asink_(alarm_.entityId(), r.cmd, r.code);
    else { aAwaiting_ = false; alarm_.sendFailed(aCodeSent_); }
    return true;
  }
  // Demo: the panel accepts kDemoAlarmCode when a code is needed (disarm always, arming when the panel says so).
  const AlarmPanelInfo info = alarmPanelInfo(st);
  const bool needCode = info.codeFormat != CodeFormat::None && (r.cmd == AlarmCmd::Disarm || info.codeArmRequired);
  if (needCode && r.code != kDemoAlarmCode) {
    aAwaiting_ = false;
    alarm_.sendFailed(true);
    return true;
  }
  if (st) {
    EntityState s = *st;
    setStateOf(s, r.cmd);
    store_.update({s});
  }
  aAwaiting_ = false;
  alarm_.sendConfirmed();
  return true;
}

bool AppState::alarmTouch(ui::Point p, uint32_t now) {
  return alarmResult(alarm_.touch(p, orientation_, alarmInfo(), now), now);
}

bool AppState::alarmRocker(ui::PhysicalKey key, uint32_t now) {
  return alarmResult(alarm_.rocker(key, orientation_, alarmInfo(), now), now);
}

bool AppState::alarmTick(uint32_t now) {
  bool changed = false;
  if (confirm_.tick(now)) changed = true;  // skipped by itself
  if (marksDriven_ && aAwaiting_ && int32_t(now - aSentAt_) >= int32_t(safetyMs_)) {
    aAwaiting_ = false;  // the driver never confirmed
    alarm_.sendFailed(aCodeSent_);
    changed = true;
  }
  if (alarm_.isOpen()) {
    const EntityState *s = store_.get(alarm_.entityId());
    if (s && !s->available) {  // the panel became unavailable: close, the code is cleared
      closeAlarm();
      changed = true;
    } else if (alarm_.tick(now)) {
      closeAlarm();
      changed = true;
    }
  }
  return changed;
}

// UNVERIFIED (device): like the media popup, the session marks the panel Pending right after the call and clears (or
// Fails) it when the confirmation fetch arrives. Failed (HA refused: wrong code) or an unchanged state after the
// confirmation shows "Wrong code" (a code was sent) / "Failed". The spec asks for 2 fetches; this checks the first.
void AppState::alarmMarks(bool &changed) {
  if (!aAwaiting_ || !alarm_.isOpen()) return;
  auto it = marks_.find(alarm_.entityId());
  const Mark m = it == marks_.end() ? Mark::None : it->second;
  if (m == Mark::Pending) {
    aSawPending_ = true;
  } else if (m == Mark::Failed) {
    aAwaiting_ = false;
    alarm_.sendFailed(aCodeSent_);
    changed = true;
  } else if (aSawPending_) {
    aAwaiting_ = false;
    aSawPending_ = false;
    const EntityState *st = store_.get(alarm_.entityId());
    if (st && st->state == aStateAtSend_) alarm_.sendFailed(aCodeSent_);
    else alarm_.sendConfirmed();
    changed = true;
  }
}

}  // namespace uistate
