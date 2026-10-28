#include "settings_ui.h"

#include <cstdio>

namespace settings_ui {

using settings::Settings;

// ---- layout ----

namespace {

void fillCommon(SettingsLayout& L, int infoX, int infoW, const int* infoY, const int* infoH) {
  for (int i = 0; i < kInfoRowCount; ++i) {
    InfoRow& r = L.info[i];
    r.rect = {infoX, infoY[i], infoW, infoH[i]};
    r.hairline = {infoX, infoY[i], infoW, 1};
    r.labelX = infoX;
    r.valueX = infoX + 158;
    r.iconX = r.valueX;
    r.cy = infoY[i] + infoH[i] / 2;
    r.cyLine1 = r.cy - 14;
    r.cyLine2 = r.cy + 14;
    r.valueWidth = infoW - 158;
  }
  for (int i = 0; i < kRowCount; ++i) L.stops[i] = L.rows[i];
  L.stops[kStopRefresh] = L.refreshButton;
  L.stops[kStopBack] = L.backButton;
}

}  // namespace

SettingsLayout computeSettingsLayout(settings::Orientation o) {
  SettingsLayout L;
  L.orientation = o;
  if (o == settings::Orientation::Portrait) {
    L.screenW = 540;
    L.screenH = 960;
    L.title = {16, 0, 200, 54};
    L.unsavedChip = {404, 11, 120, 32};
    L.headerRule = {0, 54, 540, 2};
    for (int i = 0; i < kRowCount; ++i) L.rows[i] = {16, 68 + 100 * i, 508, 88};
    L.note = {16, 468, 508, 64};
    L.refreshButton = {16, 880, 248, 64};
    L.backButton = {276, 880, 248, 64};
    const int ys[kInfoRowCount] = {548, 596, 644, 692, 740, 804};
    const int hs[kInfoRowCount] = {48, 48, 48, 48, 64, 48};
    fillCommon(L, 16, 508, ys, hs);
  } else {
    L.screenW = 960;
    L.screenH = 540;
    L.title = {16, 0, 200, 50};
    L.unsavedChip = {824, 9, 120, 32};
    L.headerRule = {0, 50, 960, 2};
    for (int i = 0; i < kRowCount; ++i) L.rows[i] = {16, 64 + 100 * i, 456, 88};
    L.note = {16, 464, 456, 64};
    L.refreshButton = {488, 464, 222, 64};
    L.backButton = {722, 464, 222, 64};
    const int ys[kInfoRowCount] = {64, 120, 176, 232, 288, 360};
    const int hs[kInfoRowCount] = {56, 56, 56, 56, 72, 56};
    fillCommon(L, 488, 456, ys, hs);
  }
  return L;
}

Rect rowLabelRect(const Rect& r) { return {r.x + 16, r.y + 18, 220, 24}; }
Rect rowHintRect(const Rect& r) { return {r.x + 16, r.y + 50, 220, 24}; }
Rect rowValueRect(const Rect& r) { return {r.x + 240, r.y, r.w - 256, r.h}; }
int rowValueRightX(const Rect& r) { return r.x + r.w - 16; }
int rowLabelCy(const Rect& r) { return r.y + 30; }
int rowHintCy(const Rect& r) { return r.y + 62; }
int rowValueCy(const Rect& r) { return r.y + 44; }
int pipCount(int i) {
  switch (i) {
    case 0: return static_cast<int>(settings::kOrientationValues.size());
    case 1: return static_cast<int>(settings::kWakeModeValues.size());
    case 2: return static_cast<int>(settings::kRefreshIntervalMinValues.size());
    case 3: return static_cast<int>(settings::kIdleTimeoutSValues.size());
    case kDemoRow: return 2;
  }
  return 0;
}
Point pipCentre(const Rect& r, int k) { return {r.x + 22 + 20 * k, r.y + 62}; }
Rect rowWarnIconRect(const Rect& r) { return {r.x + 16, r.y + 50, 24, 24}; }
int rowWarnTextX(const Rect& r) { return r.x + 46; }
Rect noteIconRect(const Rect& n) { return {n.x + 12, n.y + 12, 40, 40}; }
int noteTextX(const Rect& n) { return n.x + 64; }
int noteLineCy(const Rect& n, int line) { return n.y + (line == 0 ? 21 : 43); }
Rect buttonIconRect(const Rect& b) { return {b.x + 12, b.y + 12, 40, 40}; }
Rect buttonLabelRect(const Rect& b) { return {b.x + 60, b.y, b.w - 68, b.h}; }

// ---- text and cycling ----

const char* rowLabel(int i) {
  switch (i) {
    case 0: return "Orientation";
    case 1: return "Wake mode";
    case 2: return "Refresh every";
    case 3: return "Idle timeout";
    case kDemoRow: return "Demo mode";
  }
  return "";
}

namespace {
template <typename Arr, typename T>
int indexOf(const Arr& a, T v) {
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i] == v) return static_cast<int>(i);
  return -1;
}
template <typename Arr>
size_t nextIndex(const Arr& a, int cur) {
  return cur < 0 ? 0 : (static_cast<size_t>(cur) + 1) % a.size();
}
}  // namespace

int rowValueIndex(int i, const Settings& s) {
  switch (i) {
    case 0: return indexOf(settings::kOrientationValues, s.orientation);
    case 1: return indexOf(settings::kWakeModeValues, s.wake_mode);
    case 2: return indexOf(settings::kRefreshIntervalMinValues, s.refresh_interval_min);
    case 3: return indexOf(settings::kIdleTimeoutSValues, s.idle_timeout_s);
    case kDemoRow: return s.demo_mode ? 1 : 0;
  }
  return -1;
}

std::string rowValueText(int i, const Settings& s) {
  char buf[24];
  switch (i) {
    case 0: return s.orientation == settings::Orientation::Landscape ? "Landscape" : "Portrait";
    case 1: return s.wake_mode == settings::WakeMode::Touch ? "Touch" : "Button";
    case 2:
      if (s.refresh_interval_min == 0) return "Off";
      std::snprintf(buf, sizeof buf, "%d min", static_cast<int>(s.refresh_interval_min));
      return buf;
    case 3:
      std::snprintf(buf, sizeof buf, "%d s", static_cast<int>(s.idle_timeout_s));
      return buf;
    case kDemoRow: return s.demo_mode ? "On" : "Off";
  }
  return "";
}

Settings cycleRow(int i, const Settings& s) {
  Settings r = s;
  int cur = rowValueIndex(i, s);
  switch (i) {
    case 0: r.orientation = settings::kOrientationValues[nextIndex(settings::kOrientationValues, cur)]; break;
    case 1: r.wake_mode = settings::kWakeModeValues[nextIndex(settings::kWakeModeValues, cur)]; break;
    case 2:
      r.refresh_interval_min =
          settings::kRefreshIntervalMinValues[nextIndex(settings::kRefreshIntervalMinValues, cur)];
      break;
    case 3:
      r.idle_timeout_s = settings::kIdleTimeoutSValues[nextIndex(settings::kIdleTimeoutSValues, cur)];
      break;
    case kDemoRow: r.demo_mode = !s.demo_mode; break;
  }
  return r;
}

bool showsWakeWarning(int i, const Settings& s) {
  return i == 1 && s.wake_mode == settings::WakeMode::Touch;
}
bool showsNote(const Settings& s) { return s.wake_mode == settings::WakeMode::Touch; }

FittedText fitText(const std::string& text, int maxWidth,
                   const std::function<int(const std::string&, int)>& measure) {
  if (measure(text, 24) <= maxWidth) return {text, 24};
  if (measure(text, 18) <= maxWidth) return {text, 18};
  std::string t = text;
  while (!t.empty()) {
    // drop one UTF-8 character
    size_t n = t.size() - 1;
    while (n > 0 && (static_cast<unsigned char>(t[n]) & 0xC0) == 0x80) --n;
    t.resize(n);
    std::string cand = t + "\xE2\x80\xA6";
    if (measure(cand, 18) <= maxWidth) return {cand, 18};
  }
  return {"\xE2\x80\xA6", 18};
}

// ---- info text ----

std::string formatTime(const WallTime& t, const WallTime& now) {
  char buf[32];
  if (t.day == now.day && t.month == now.month) {
    std::snprintf(buf, sizeof buf, "%02d:%02d", t.hour, t.minute);
  } else {
    std::snprintf(buf, sizeof buf, "%d.%d. %02d:%02d", t.day, t.month, t.hour, t.minute);
  }
  return buf;
}

InfoText batteryText(const BatteryInfo& b) {
  char buf[32];
  int v = (b.millivolts + 5) / 10;  // 10 mV steps
  std::snprintf(buf, sizeof buf, "%d%%, %d.%02d V", b.percent, v / 100, v % 100);
  InfoText r;
  r.text = buf;
  if (b.charging) r.icon = Icon::Bolt;
  else if (b.percent <= kLowBatteryPercent) r.icon = Icon::Alert;
  return r;
}

InfoText haText(const HaInfo& h, const WallTime& now) {
  InfoText r;
  if (h.refreshing) {
    r.icon = Icon::Hourglass;
    r.text = "refreshing\xE2\x80\xA6";
    return r;
  }
  if (!h.sessionDone) {
    r.text = "-";
    return r;
  }
  if (h.lastAttemptOk) {
    r.text = std::string(h.lastLink == HaInfo::Link::Remote ? "remote" : "local") + ", ok " +
             formatTime(h.lastOk, now);
    return r;
  }
  r.icon = Icon::Alert;
  r.text = h.everOk ? "offline, last ok " + formatTime(h.lastOk, now) : "offline, never ok";
  return r;
}

InfoText wifiText(const WifiInfo& w) {
  InfoText r;
  if (!w.configured) r.text = "-";
  else r.text = w.connected ? w.ssid : w.ssid + " (not connected)";
  return r;
}

InfoText configText(const ConfigInfo& c) {
  InfoText r;
  if (c.missing) {
    r.icon = Icon::Warn;
    r.text = "no config.yaml on SD";
  } else if (c.errorCount > 0) {
    r.icon = Icon::Alert;
    r.text = c.firstPath;
    if (c.firstLine > 0) r.text += " (line " + std::to_string(c.firstLine) + ")";
  } else {
    r.text = "OK";
  }
  return r;
}

std::string errorsText(const ConfigInfo& c) {
  return c.missing ? "-" : std::to_string(c.errorCount);
}

// ---- state machine ----

SettingsScreen::SettingsScreen(const Settings& saved, settings::Orientation current,
                               bool ringVisible)
    : saved_(saved),
      edit_(saved),
      layout_(computeSettingsLayout(current)),
      ringVisible_(ringVisible) {}

Update SettingsScreen::activate(int stop) {
  Update u;
  if (stop == kStopBack) {
    u.action = Action::Leave;
  } else if (stop == kStopRefresh) {
    if (refresh_ == RefreshState::Running) return u;
    refresh_ = RefreshState::Running;
    u.action = Action::StartRefresh;
    u.dirty.push_back(layout_.refreshButton);
    u.dirty.push_back(layout_.info[static_cast<int>(InfoId::HomeAssistant)].rect);
  } else if (stop >= 0 && stop < kRowCount) {
    const bool wasDirty = dirty();
    const bool hadNote = showsNote(edit_);
    edit_ = cycleRow(stop, edit_);
    const Rect& row = layout_.rows[stop];
    u.dirty.push_back(rowValueRect(row));
    u.dirty.push_back(rowHintRect(row));
    if (hadNote != showsNote(edit_)) u.dirty.push_back(layout_.note);
    if (wasDirty != dirty()) u.dirty.push_back(layout_.unsavedChip);
  }
  return u;
}

Update SettingsScreen::onTouch(Point p) {
  for (int i = 0; i < kFocusStops; ++i)
    if (layout_.stops[i].contains(p)) return activate(i);
  return {};
}

Update SettingsScreen::onKey(ui::LogicalKey key) {
  Update u;
  switch (key) {
    case ui::LogicalKey::Prev:
    case ui::LogicalKey::Next: {
      const int old = focus_;
      focus_ = key == ui::LogicalKey::Next ? (focus_ + 1) % kFocusStops
                                           : (focus_ + kFocusStops - 1) % kFocusStops;
      if (ringVisible_) u.dirty.push_back(ui::inflate(layout_.stops[old], kRingInflate));
      u.dirty.push_back(ui::inflate(layout_.stops[focus_], kRingInflate));
      ringVisible_ = true;
      break;
    }
    case ui::LogicalKey::Push:
      u = activate(focus_);
      break;
    case ui::LogicalKey::LongPush:
      u.action = Action::Leave;
      break;
  }
  return u;
}

Update SettingsScreen::refreshDone(bool ok) {
  Update u;
  if (refresh_ != RefreshState::Running) return u;
  refresh_ = ok ? RefreshState::Idle : RefreshState::Failed;
  u.dirty.push_back(layout_.refreshButton);
  u.dirty.push_back(layout_.info[static_cast<int>(InfoId::HomeAssistant)].rect);
  if (ok) u.dirty.push_back(layout_.info[static_cast<int>(InfoId::Battery)].rect);
  return u;
}

LeaveResult SettingsScreen::leave() const {
  LeaveResult r;
  r.save = edit_ != saved_;
  r.orientationChanged = edit_.orientation != saved_.orientation;
  r.demoChanged = edit_.demo_mode != saved_.demo_mode;
  r.settings = edit_;
  return r;
}

}  // namespace settings_ui
