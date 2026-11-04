#include "alarm_popup.h"

#include <algorithm>

namespace uistate {

using settings::Orientation;

AlarmLayout alarmLayout(Orientation o) {
  AlarmLayout l;
  if (o == Orientation::Landscape) {
    l.landscape = true;
    l.panel = {40, 94, 880, 408};
    l.close = {840, 110, 64, 64};
    l.codeBox = {56, 262, 300, 64};
    const int kx[3] = {380, 488, 596}, ky[4] = {186, 262, 338, 414};
    for (int i = 0; i < 12; ++i) l.keys[i] = {kx[i % 3], ky[i / 3], 96, 64};
    for (int i = 0; i < 4; ++i) l.modes[i] = {728, 186 + 76 * i, 176, 64};
    l.nameX = 56; l.nameCy = 142;
    l.stateX = 56; l.stateCy = 222; l.stateBaseline = 236;
    l.codeTextX = 72; l.codeCy = 294;
    l.msgX = 56; l.msgCy = 358;
    l.hintX = 56; l.hint1Cy = 424; l.hint2Cy = 450;
  } else {
    l.landscape = false;
    l.panel = {16, 116, 508, 704};
    const int px = l.panel.x, py = l.panel.y;  // portrait numbers are panel-relative in the spec
    l.close = {px + 428, py + 16, 64, 64};
    l.codeBox = {px + 16, py + 152, 476, 64};
    const int kx[3] = {16, 179, 342}, ky[4] = {264, 340, 416, 492};
    for (int i = 0; i < 12; ++i) l.keys[i] = {px + kx[i % 3], py + ky[i / 3], 150, 64};
    const int mx[4] = {16, 138, 260, 382};
    for (int i = 0; i < 4; ++i) l.modes[i] = {px + mx[i], py + 572, 110, 64};
    l.nameX = px + 16; l.nameCy = py + 48;
    l.stateX = px + 16; l.stateCy = py + 120; l.stateBaseline = py + 134;
    l.codeTextX = px + 32; l.codeCy = py + 184;
    l.msgX = px + 16; l.msgCy = py + 242;
    l.hintX = px + 16; l.hint1Cy = py + 668; l.hint2Cy = py + 694;
  }
  l.shadow = {l.panel.x + 6, l.panel.y + 6, l.panel.w, l.panel.h};
  l.dirty = {l.panel.x - 2, l.panel.y - 2, l.panel.w + 10, l.panel.h + 10};
  l.keysArea = {l.keys[0].x, l.keys[0].y, l.keys[2].x + l.keys[2].w - l.keys[0].x, l.keys[9].y + 64 - l.keys[0].y};
  return l;
}

const char *alarmMsgText(AlarmMsg m) {
  switch (m) {
    case AlarmMsg::EnterCode: return "Enter code";
    case AlarmMsg::CodeEntered: return "Code entered, pick a mode";
    case AlarmMsg::WrongCode: return "Wrong code";
    case AlarmMsg::Failed: return "Failed";
    case AlarmMsg::Sent: return "Sent";
    case AlarmMsg::TextUnsupported: return "Text code not supported";
  }
  return "";
}

AlarmHit alarmHitTest(const AlarmLayout &l, ui::Point p) {
  AlarmHit h;
  if (!l.panel.contains(p)) return h;
  h.kind = AlarmHitKind::Panel;
  if (l.close.contains(p)) { h.kind = AlarmHitKind::Close; return h; }
  for (int i = 0; i < 12; ++i)
    if (l.keys[i].contains(p)) { h.kind = AlarmHitKind::Key; h.index = i; return h; }
  for (int i = 0; i < 4; ++i)
    if (l.modes[i].contains(p)) { h.kind = AlarmHitKind::Mode; h.index = i; return h; }
  if (l.codeBox.contains(p)) h.kind = AlarmHitKind::CodeBox;
  return h;
}

void AlarmPopup::open(const std::string &entityId, int tab, int widget, bool viaRocker, uint32_t now) {
  open_ = true;
  id_ = entityId;
  tab_ = tab;
  widget_ = widget;
  code_.clear();
  ring_ = viaRocker;
  focus_ = kEntryFocus;
  awaiting_ = -1;
  hasOverride_ = false;
  lastInput_ = now;
}

void AlarmPopup::close() {
  open_ = false;
  ring_ = false;
  code_.clear();
  awaiting_ = -1;
  hasOverride_ = false;
}

bool AlarmPopup::modeActive(int mode, const AlarmPanelInfo &info) const {
  if (info.unavailable) return false;
  switch (mode) {
    case 0: return true;
    case 1: return info.canHome;
    case 2: return info.canAway;
    case 3: return info.canNight;
  }
  return false;
}

std::vector<int> AlarmPopup::stops(const AlarmPanelInfo &info) const {
  std::vector<int> v;
  if (keysActive(info))
    for (int i = 0; i < kStopKeys; ++i) v.push_back(i);
  for (int m = 0; m < kStopModes; ++m)
    if (modeActive(m, info)) v.push_back(kStopKeys + m);
  v.push_back(kStopClose);
  return v;
}

AlarmMsg AlarmPopup::message(const AlarmPanelInfo &info) const {
  if (info.codeFormat == CodeFormat::Text) return AlarmMsg::TextUnsupported;
  if (hasOverride_) return override_;
  return code_.empty() ? AlarmMsg::EnterCode : AlarmMsg::CodeEntered;
}

bool AlarmPopup::keyPress(int key, const AlarmPanelInfo &info) {
  if (!keysActive(info)) return false;
  hasOverride_ = false;  // a key press replaces Wrong code / Failed / Sent
  if (key == kKeyBackspace) {
    if (code_.empty()) return true;
    code_.pop_back();
  } else if (key == kKeyClear) {
    code_.clear();
  } else {
    if (int(code_.size()) >= kAlarmMaxCode) return true;
    code_.push_back(char('0' + (key == kKeyZero ? 0 : key + 1)));
  }
  return true;
}

AlarmResult AlarmPopup::press(int mode, const AlarmPanelInfo &info) {
  AlarmResult r;
  if (!modeActive(mode, info) || awaiting_ >= 0) return r;
  const bool hasFormat = info.codeFormat != CodeFormat::None;
  const bool needCode = hasFormat && (mode == 0 || info.codeArmRequired);
  if (info.codeFormat == CodeFormat::Text && needCode) return r;  // message already says so
  if (needCode && code_.empty()) {
    hasOverride_ = true;
    override_ = AlarmMsg::EnterCode;
    r.changed = true;
    return r;
  }
  r.send = true;
  r.mode = mode;
  r.cmd = mode == 0 ? AlarmCmd::Disarm : mode == 1 ? AlarmCmd::ArmHome : mode == 2 ? AlarmCmd::ArmAway : AlarmCmd::ArmNight;
  if (hasFormat) r.code = code_;
  clearCode();  // cleared on send
  awaiting_ = mode;
  hasOverride_ = true;
  override_ = AlarmMsg::Sent;
  r.changed = true;
  return r;
}

AlarmResult AlarmPopup::touch(ui::Point p, Orientation o, const AlarmPanelInfo &info, uint32_t now) {
  AlarmResult r;
  if (!open_) return r;
  lastInput_ = now;
  ring_ = false;
  const AlarmHit h = alarmHitTest(alarmLayout(o), p);
  switch (h.kind) {
    case AlarmHitKind::Outside:
    case AlarmHitKind::Close:
      close();
      r.changed = r.closed = true;
      return r;
    case AlarmHitKind::Key: r.changed = keyPress(h.index, info); return r;
    case AlarmHitKind::Mode: return press(h.index, info);
    default: return r;
  }
}

AlarmResult AlarmPopup::rocker(ui::PhysicalKey key, Orientation o, const AlarmPanelInfo &info, uint32_t now) {
  AlarmResult r;
  if (!open_) return r;
  lastInput_ = now;
  if (key == ui::PhysicalKey::LongPush) {
    close();
    r.changed = r.closed = true;
    return r;
  }
  const std::vector<int> st = stops(info);
  if (key == ui::PhysicalKey::Push) {
    if (!ring_) {  // first event reveals the ring on key "5" (or the first stop)
      ring_ = true;
      r.changed = true;
      return r;
    }
    if (focus_ == kStopClose) {
      close();
      r.changed = r.closed = true;
      return r;
    }
    if (focus_ >= kStopKeys) return press(focus_ - kStopKeys, info);
    r.changed = keyPress(focus_, info);
    return r;
  }
  const bool prev = ui::mapRocker(key, o, false) == ui::LogicalKey::Prev;
  if (!ring_) {
    ring_ = true;
    r.changed = true;
    if (std::find(st.begin(), st.end(), focus_) == st.end()) focus_ = st.front();
    return r;
  }
  auto it = std::find(st.begin(), st.end(), focus_);
  size_t i = it == st.end() ? 0 : size_t(it - st.begin());
  i = prev ? (i + st.size() - 1) % st.size() : (i + 1) % st.size();  // wraps
  focus_ = st[i];
  r.changed = true;
  return r;
}

bool AlarmPopup::tick(uint32_t now) {
  if (!open_ || int32_t(now - lastInput_) < int32_t(kAlarmIdleCloseMs)) return false;
  close();
  return true;
}

void AlarmPopup::sendFailed(bool codeWasSent) {
  awaiting_ = -1;
  hasOverride_ = true;
  override_ = codeWasSent ? AlarmMsg::WrongCode : AlarmMsg::Failed;
  clearCode();  // cleared after a wrong-code reply
}

void AlarmPopup::sendConfirmed() {
  awaiting_ = -1;
  hasOverride_ = true;
  override_ = AlarmMsg::Sent;
}

}  // namespace uistate
