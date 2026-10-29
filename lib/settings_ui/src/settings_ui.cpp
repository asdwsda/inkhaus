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
  L.stops[kStopTools] = L.toolsRow;
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
    for (int i = 0; i < kRowCount; ++i) L.rows[i] = {16, 68 + 84 * i, 508, 72};
    L.toolsRow = {16, 404, 508, 64};
    L.note = {16, 480, 508, 64};
    L.refreshButton = {16, 880, 248, 64};
    L.backButton = {276, 880, 248, 64};
    const int ys[kInfoRowCount] = {560, 608, 656, 704, 752, 816};
    const int hs[kInfoRowCount] = {48, 48, 48, 48, 64, 48};
    fillCommon(L, 16, 508, ys, hs);
  } else {
    L.screenW = 960;
    L.screenH = 540;
    L.title = {16, 0, 200, 50};
    L.unsavedChip = {824, 9, 120, 32};
    L.headerRule = {0, 50, 960, 2};
    for (int i = 0; i < kRowCount; ++i) L.rows[i] = {16, 64 + 80 * i, 456, 68};
    L.toolsRow = {16, 384, 456, 64};
    L.note = {16, 460, 456, 64};
    L.refreshButton = {488, 464, 222, 64};
    L.backButton = {722, 464, 222, 64};
    const int ys[kInfoRowCount] = {64, 120, 176, 232, 288, 360};
    const int hs[kInfoRowCount] = {56, 56, 56, 56, 72, 56};
    fillCommon(L, 488, 456, ys, hs);
  }
  return L;
}

ToolsLayout computeToolsLayout(settings::Orientation o) {
  ToolsLayout L;
  L.orientation = o;
  if (o == settings::Orientation::Portrait) {
    L.screenW = 540;
    L.screenH = 960;
    L.title = {16, 0, 200, 54};
    L.unsavedChip = {404, 11, 120, 32};
    L.headerRule = {0, 54, 540, 2};
    L.rows[0] = {16, 68, 508, 88};
    L.rows[1] = {16, 168, 508, 88};
    L.note = {16, 268, 508, 64};
    L.backButton = {276, 880, 248, 64};
  } else {
    L.screenW = 960;
    L.screenH = 540;
    L.title = {16, 0, 200, 50};
    L.unsavedChip = {824, 9, 120, 32};
    L.headerRule = {0, 50, 960, 2};
    L.rows[0] = {16, 64, 456, 88};
    L.rows[1] = {488, 64, 456, 88};
    L.note = {488, 164, 456, 64};
    L.backButton = {722, 464, 222, 64};
  }
  L.stops[0] = L.rows[0];
  L.stops[1] = L.rows[1];
  L.stops[kToolsStopBack] = L.backButton;
  return L;
}

Rect rowLabelRect(const Rect& r) { return {r.x + 16, r.y + r.h / 2 - 24, 220, 28}; }
Rect rowHintRect(const Rect& r) { return {r.x + 16, r.y + r.h / 2 + 4, 220, 18}; }
Rect rowValueRect(const Rect& r) { return {r.x + 240, r.y, r.w - 256, r.h}; }
int rowValueRightX(const Rect& r) { return r.x + r.w - 16; }
int rowLabelCy(const Rect& r) { return r.y + r.h / 2 - 10; }
int rowHintCy(const Rect& r) { return r.y + r.h / 2 + 13; }
int rowValueCy(const Rect& r) { return r.y + r.h / 2; }
int toolsRowLabelX(const Rect& r) { return r.x + 16; }
int toolsRowCy(const Rect& r) { return r.y + 32; }
int toolsRowSummaryRightX(const Rect& r) { return r.x + r.w - 64; }
Rect toolsRowChevronRect(const Rect& r) { return {r.x + r.w - 52, r.y + 12, 40, 40}; }
std::string toolsSummary(const Settings& s) {
  return std::string("Demo ") + (s.demo_mode ? "On" : "Off") + ", Log " + (s.debug_log ? "On" : "Off");
}
int toolsRowLabelCy(const Rect& r) { return r.y + 32; }
int toolsRowHintCy(const Rect& r) { return r.y + 62; }
int toolsRowValueCy(const Rect& r) { return r.y + 44; }
int toolsRowValueRightX(const Rect& r) { return r.x + r.w - 16; }
Rect toolsRowValueRect(const Rect& r) { return {r.x + r.w - 156, r.y, 156, r.h}; }
const char* toolsRowLabel(int i) { return i == kToolsDemoRow ? "Demo mode" : i == kToolsDebugRow ? "Debug log" : ""; }
const char* toolsRowHint(int i) {
  return i == kToolsDemoRow ? "Sample dashboard, offline" : i == kToolsDebugRow ? "Log lines to SD card" : "";
}
std::string toolsRowValueText(int i, const Settings& s) {
  const bool on = i == kToolsDemoRow ? s.demo_mode : i == kToolsDebugRow ? s.debug_log : false;
  return on ? "On" : "Off";
}
Settings toggleToolsRow(int i, const Settings& s) {
  Settings r = s;
  if (i == kToolsDemoRow) r.demo_mode = !s.demo_mode;
  else if (i == kToolsDebugRow) r.debug_log = !s.debug_log;
  return r;
}
bool toolsShowsNote(const Settings& s) { return s.debug_log; }
int pipCount(int i) {
  switch (i) {
    case 0: return static_cast<int>(settings::kOrientationValues.size());
    case 1: return static_cast<int>(settings::kWakeModeValues.size());
    case 2: return static_cast<int>(settings::kRefreshIntervalMinValues.size());
    case 3: return static_cast<int>(settings::kIdleTimeoutSValues.size());
  }
  return 0;
}
Point pipCentre(const Rect& r, int k) { return {r.x + 22 + 20 * k, rowHintCy(r)}; }
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
  }
  return -1;
}

std::string rowValueText(int i, const Settings& s) {
  char buf[24];
  switch (i) {
    case 0: return s.orientation == settings::Orientation::Landscape ? "Landscape" : "Portrait";
    case 1:
      return s.wake_mode == settings::WakeMode::Touch            ? "Touch"
             : s.wake_mode == settings::WakeMode::TouchAndButton ? "Touch & Button"
                                                                 : "Button";
    case 2:
      if (s.refresh_interval_min == 0) return "Off";
      std::snprintf(buf, sizeof buf, "%d min", static_cast<int>(s.refresh_interval_min));
      return buf;
    case 3:
      std::snprintf(buf, sizeof buf, "%d s", static_cast<int>(s.idle_timeout_s));
      return buf;
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
  }
  return r;
}

bool showsNote(const Settings& s) { return s.wake_mode != settings::WakeMode::Button; }

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
      tools_(computeToolsLayout(current)),
      ringVisible_(ringVisible) {}

Update SettingsScreen::activate(int stop) {
  Update u;
  if (stop == kStopBack) {
    u.action = Action::Leave;
  } else if (stop == kStopTools) {
    page_ = Page::Tools;
    toolsFocus_ = 0;
    u.action = Action::PageChange;
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
    // The Tools row summary does not change here (it shows demo/log only).
  }
  return u;
}

Update SettingsScreen::toolsBack() {
  Update u;
  page_ = Page::Main;
  focus_ = kStopTools;  // the main page focus returns to the Tools row
  u.action = Action::PageChange;
  return u;
}

Update SettingsScreen::activateTools(int stop) {
  Update u;
  if (stop == kToolsStopBack) return toolsBack();
  if (stop < 0 || stop >= kToolsRowCount) return u;
  const bool wasDirty = dirty();
  const bool hadNote = toolsShowsNote(edit_);
  edit_ = toggleToolsRow(stop, edit_);
  u.dirty.push_back(toolsRowValueRect(tools_.rows[stop]));
  if (hadNote != toolsShowsNote(edit_)) u.dirty.push_back(tools_.note);
  if (wasDirty != dirty()) u.dirty.push_back(tools_.unsavedChip);
  return u;
}

Update SettingsScreen::onTouch(Point p) {
  if (page_ == Page::Tools) {
    for (int i = 0; i < kToolsFocusStops; ++i)
      if (tools_.stops[i].contains(p)) return activateTools(i);
    return {};
  }
  for (int i = 0; i < kFocusStops; ++i)
    if (layout_.stops[i].contains(p)) return activate(i);
  return {};
}

Update SettingsScreen::onKey(ui::LogicalKey key) {
  Update u;
  const bool tools = page_ == Page::Tools;
  const int n = stopCount();
  switch (key) {
    case ui::LogicalKey::Prev:
    case ui::LogicalKey::Next: {
      int& f = tools ? toolsFocus_ : focus_;
      const Rect* stops = tools ? tools_.stops : layout_.stops;
      const int old = f;
      f = key == ui::LogicalKey::Next ? (f + 1) % n : (f + n - 1) % n;
      if (ringVisible_) u.dirty.push_back(ui::inflate(stops[old], kRingInflate));
      u.dirty.push_back(ui::inflate(stops[f], kRingInflate));
      ringVisible_ = true;
      break;
    }
    case ui::LogicalKey::Push:
      u = tools ? activateTools(toolsFocus_) : activate(focus_);
      break;
    case ui::LogicalKey::LongPush:
      if (tools) u = toolsBack();  // one level up, not out of Settings
      else u.action = Action::Leave;
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
  r.debugLogChanged = edit_.debug_log != saved_.debug_log;
  r.settings = edit_;
  return r;
}

}  // namespace settings_ui
