#include "settings_ui.h"

#include <cstdio>

namespace settings_ui {

using settings::Settings;

// ---- layout ----

const char* categoryLabel(int i) {
  switch (i) {
    case 0: return "Display";
    case 1: return "Sleep & wake";
    case 2: return "Network";
    case 3: return "Tools";
  }
  return "";
}

bool isPipRow(RowId r) { return r == RowId::Orientation || r == RowId::Wake || r == RowId::Refresh || r == RowId::Idle; }

namespace {

void fillInfo(SettingsLayout& L, int infoX, int infoW, const int* infoY, const int* infoH) {
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
  for (int i = 0; i < kCategoryCount; ++i) L.stops[i] = L.categories[i];
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
    for (int i = 0; i < kCategoryCount; ++i) L.categories[i] = {16, 68 + 108 * i, 508, 96};
    L.refreshButton = {16, 880, 248, 64};
    L.backButton = {276, 880, 248, 64};
    const int ys[kInfoRowCount] = {504, 552, 600, 648, 696, 760};
    const int hs[kInfoRowCount] = {48, 48, 48, 48, 64, 48};
    fillInfo(L, 16, 508, ys, hs);
  } else {
    L.screenW = 960;
    L.screenH = 540;
    L.title = {16, 0, 200, 50};
    L.unsavedChip = {824, 9, 120, 32};
    L.headerRule = {0, 50, 960, 2};
    for (int i = 0; i < kCategoryCount; ++i) L.categories[i] = {16, 64 + 108 * i, 456, 96};
    L.refreshButton = {488, 464, 222, 64};
    L.backButton = {722, 464, 222, 64};
    const int ys[kInfoRowCount] = {64, 120, 176, 232, 288, 360};
    const int hs[kInfoRowCount] = {56, 56, 56, 56, 72, 56};
    fillInfo(L, 488, 456, ys, hs);
  }
  return L;
}

PageLayout computePageLayout(Page p, settings::Orientation o) {
  PageLayout L;
  L.page = p;
  L.orientation = o;
  const bool portrait = o == settings::Orientation::Portrait;
  L.screenW = portrait ? 540 : 960;
  L.screenH = portrait ? 960 : 540;
  L.title = {16, 0, 200, portrait ? 54 : 50};
  L.unsavedChip = portrait ? Rect{404, 11, 120, 32} : Rect{824, 9, 120, 32};
  L.headerRule = portrait ? Rect{0, 54, 540, 2} : Rect{0, 50, 960, 2};
  L.backButton = portrait ? Rect{276, 880, 248, 64} : Rect{722, 464, 222, 64};
  const int w = portrait ? 508 : 456;
  const int y0 = portrait ? 68 : 64;
  switch (p) {
    case Page::Display:
      L.rowCount = 1;
      L.rowIds[0] = RowId::Orientation;
      L.rows[0] = {16, y0, w, portrait ? 72 : 68};
      L.caption = portrait ? Rect{16, 152, 508, 32} : Rect{16, 132, 456, 32};
      break;
    case Page::Sleep: {
      const int pitch = portrait ? 84 : 80;
      const int h = portrait ? 72 : 68;
      L.rowCount = 3;
      L.rowIds[0] = RowId::Wake;
      L.rowIds[1] = RowId::Refresh;
      L.rowIds[2] = RowId::Idle;
      for (int i = 0; i < 3; ++i) L.rows[i] = {16, y0 + pitch * i, w, h};
      L.note = portrait ? Rect{16, 320, 508, 64} : Rect{16, 304, 456, 64};
      break;
    }
    case Page::Network:
      L.rowCount = 2;
      L.rowIds[0] = RowId::Wifi;
      L.rowIds[1] = RowId::HaLink;
      if (portrait) {
        L.rows[0] = {16, 68, 508, 88};
        L.rows[1] = {16, 168, 508, 88};
        L.note = {16, 268, 508, 64};
      } else {
        L.rows[0] = {16, 64, 456, 88};
        L.rows[1] = {488, 64, 456, 88};
        L.note = {16, 164, 456, 64};  // under Wi-Fi
      }
      break;
    case Page::Tools:
      L.rowCount = 2;
      L.rowIds[0] = RowId::Demo;
      L.rowIds[1] = RowId::Debug;
      if (portrait) {
        L.rows[0] = {16, 68, 508, 88};
        L.rows[1] = {16, 168, 508, 88};
        L.note = {16, 268, 508, 64};
      } else {
        L.rows[0] = {16, 64, 456, 88};
        L.rows[1] = {488, 64, 456, 88};
        L.note = {488, 164, 456, 64};
      }
      break;
    case Page::Main:
      break;
  }
  for (int i = 0; i < L.rowCount; ++i) L.stops[i] = L.rows[i];
  L.stops[L.rowCount] = L.backButton;
  L.stopCount = L.rowCount + 1;
  return L;
}

Rect rowLabelRect(const Rect& r) { return {r.x + 16, r.y + r.h / 2 - 24, 220, 28}; }
Rect rowHintRect(const Rect& r) { return {r.x + 16, r.y + r.h / 2 + 4, 220, 18}; }
Rect rowValueRect(const Rect& r) { return {r.x + 240, r.y, r.w - 256, r.h}; }
int rowValueRightX(const Rect& r) { return r.x + r.w - 16; }
int rowLabelCy(const Rect& r) { return r.y + r.h / 2 - 10; }
int rowHintCy(const Rect& r) { return r.y + r.h / 2 + 13; }
int rowValueCy(const Rect& r) { return r.y + r.h / 2; }

int categoryLabelX(const Rect& r) { return r.x + 16; }
int categoryLabelCy(const Rect& r) { return r.y + 34; }
int categorySummaryCy(const Rect& r) { return r.y + 66; }
int categorySummaryMaxWidth(const Rect& r) { return r.w - 32 - 52; }
Rect categoryChevronRect(const Rect& r) { return {r.x + r.w - 52, r.y + 28, 40, 40}; }

int hintRowLabelCy(const Rect& r) { return r.y + 32; }
int hintRowHintCy(const Rect& r) { return r.y + 62; }
int hintRowValueCy(const Rect& r) { return r.y + 44; }
Rect hintRowValueRect(const Rect& r) { return {r.x + r.w - 268, r.y, 268, r.h}; }
int hintRowValueWidth() { return 252; }
const char* hintRowLabel(RowId r) {
  switch (r) {
    case RowId::Wifi: return "Wi-Fi";
    case RowId::HaLink: return "HA connection";
    case RowId::Demo: return "Demo mode";
    case RowId::Debug: return "Debug log";
    default: return "";
  }
}
const char* hintRowStaticHint(RowId r) {
  switch (r) {
    case RowId::Demo: return "Sample dashboard, offline";
    case RowId::Debug: return "Log lines to SD card";
    default: return "";
  }
}
Point pipCentre(const Rect& r, int k) { return {r.x + 22 + 20 * k, rowHintCy(r)}; }
Rect noteIconRect(const Rect& n) { return {n.x + 12, n.y + 12, 40, 40}; }
int noteTextX(const Rect& n) { return n.x + 64; }
int noteLineCy(const Rect& n, int line) { return n.y + (line == 0 ? 21 : 43); }
Rect buttonIconRect(const Rect& b) { return {b.x + 12, b.y + 12, 40, 40}; }
Rect buttonLabelRect(const Rect& b) { return {b.x + 60, b.y, b.w - 68, b.h}; }

// ---- text and cycling ----

const char* rowLabel(RowId r) {
  switch (r) {
    case RowId::Orientation: return "Orientation";
    case RowId::Wake: return "Wake mode";
    case RowId::Refresh: return "Refresh every";
    case RowId::Idle: return "Idle timeout";
    default: return hintRowLabel(r);
  }
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
const char* haLinkName(settings::HaLink l) {
  return l == settings::HaLink::Internal ? "Internal" : l == settings::HaLink::External ? "External" : "Auto";
}
std::string wifiValue(const Settings& s, const NetworkInfo& n) {
  const int p = s.wifi_profile;
  if (p <= 0) return "Auto";
  if (static_cast<size_t>(p) <= n.profiles.size()) return n.profiles[static_cast<size_t>(p) - 1];
  return "Profile " + std::to_string(p);  // a profile that is no longer in secrets.yaml
}
}  // namespace

int pipCount(RowId r) {
  switch (r) {
    case RowId::Orientation: return static_cast<int>(settings::kOrientationValues.size());
    case RowId::Wake: return static_cast<int>(settings::kWakeModeValues.size());
    case RowId::Refresh: return static_cast<int>(settings::kRefreshIntervalMinValues.size());
    case RowId::Idle: return static_cast<int>(settings::kIdleTimeoutSValues.size());
    default: return 0;
  }
}

int rowValueIndex(RowId r, const Settings& s) {
  switch (r) {
    case RowId::Orientation: return indexOf(settings::kOrientationValues, s.orientation);
    case RowId::Wake: return indexOf(settings::kWakeModeValues, s.wake_mode);
    case RowId::Refresh: return indexOf(settings::kRefreshIntervalMinValues, s.refresh_interval_min);
    case RowId::Idle: return indexOf(settings::kIdleTimeoutSValues, s.idle_timeout_s);
    default: return -1;
  }
}

std::string rowValueText(RowId r, const Settings& s, const NetworkInfo& n) {
  char buf[24];
  switch (r) {
    case RowId::Orientation: return s.orientation == settings::Orientation::Landscape ? "Landscape" : "Portrait";
    case RowId::Wake:
      return s.wake_mode == settings::WakeMode::Touch            ? "Touch"
             : s.wake_mode == settings::WakeMode::TouchAndButton ? "Touch & Button"
                                                                 : "Button";
    case RowId::Refresh:
      if (s.refresh_interval_min == 0) return "Off";
      std::snprintf(buf, sizeof buf, "%d min", static_cast<int>(s.refresh_interval_min));
      return buf;
    case RowId::Idle:
      std::snprintf(buf, sizeof buf, "%d s", static_cast<int>(s.idle_timeout_s));
      return buf;
    case RowId::Wifi: return wifiValue(s, n);
    case RowId::HaLink: return haLinkName(s.ha_link);
    case RowId::Demo: return s.demo_mode ? "On" : "Off";
    case RowId::Debug: return s.debug_log ? "On" : "Off";
  }
  return "";
}

Settings cycleRow(RowId r, const Settings& s, const NetworkInfo& n) {
  Settings o = s;
  const int cur = rowValueIndex(r, s);
  switch (r) {
    case RowId::Orientation: o.orientation = settings::kOrientationValues[nextIndex(settings::kOrientationValues, cur)]; break;
    case RowId::Wake: o.wake_mode = settings::kWakeModeValues[nextIndex(settings::kWakeModeValues, cur)]; break;
    case RowId::Refresh:
      o.refresh_interval_min =
          settings::kRefreshIntervalMinValues[nextIndex(settings::kRefreshIntervalMinValues, cur)];
      break;
    case RowId::Idle:
      o.idle_timeout_s = settings::kIdleTimeoutSValues[nextIndex(settings::kIdleTimeoutSValues, cur)];
      break;
    case RowId::Wifi: {
      const int count = static_cast<int>(n.profiles.size());
      o.wifi_profile = (s.wifi_profile < 0 || s.wifi_profile >= count) ? 0 : s.wifi_profile + 1;
      break;
    }
    case RowId::HaLink:
      o.ha_link = s.ha_link == settings::HaLink::Auto       ? settings::HaLink::Internal
                  : s.ha_link == settings::HaLink::Internal ? settings::HaLink::External
                                                            : settings::HaLink::Auto;
      break;
    case RowId::Demo: o.demo_mode = !s.demo_mode; break;
    case RowId::Debug: o.debug_log = !s.debug_log; break;
  }
  return o;
}

std::string wifiHint(const NetworkInfo& n) {
  if (n.demo) return "Not used (demo mode)";
  return n.connected.empty() ? "Not connected" : "Connected: " + n.connected;
}

std::string haLinkHint(const Settings& edit, const NetworkInfo& n) {
  if (n.demo) return "Not used (demo mode)";
  switch (n.link) {
    case LinkState::None: return "Link: none (offline)";
    case LinkState::Unknown: return "Link: -";
    default: break;
  }
  std::string t = n.link == LinkState::External ? "Link: external" : "Link: internal";
  if (edit.ha_link == settings::HaLink::Auto && n.profileDecides) t += " (profile)";
  return t;
}

std::string categorySummary(int category, const Settings& s, const NetworkInfo& n, bool shortWake) {
  switch (category) {
    case 0: return rowValueText(RowId::Orientation, s, n);
    case 1: {
      std::string wake = rowValueText(RowId::Wake, s, n);
      if (shortWake && s.wake_mode == settings::WakeMode::TouchAndButton) wake = "Both";
      return wake + ", " + rowValueText(RowId::Refresh, s, n) + ", " + rowValueText(RowId::Idle, s, n);
    }
    case 2: return "Wi-Fi " + wifiValue(s, n) + ", HA " + haLinkName(s.ha_link);
    case 3: return std::string("Demo ") + (s.demo_mode ? "On" : "Off") + ", Log " + (s.debug_log ? "On" : "Off");
  }
  return "";
}

bool pageShowsNote(Page p, const Settings& s) {
  switch (p) {
    case Page::Sleep: return s.wake_mode != settings::WakeMode::Button;
    case Page::Network: return s.wifi_profile != 0;
    case Page::Tools: return s.debug_log;
    default: return false;
  }
}

NoteText pageNote(Page p, bool sdPresent) {
  switch (p) {
    case Page::Sleep: return {kNoteLine1, kNoteLine2};
    case Page::Network: return {kNetNoteLine1, kNetNoteLine2};
    case Page::Tools:
      return sdPresent ? NoteText{kToolsNoteLine1, kToolsNoteLine2} : NoteText{kToolsNoCardLine1, kToolsNoCardLine2};
    default: return {"", ""};
  }
}

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
    r.text = std::string(h.lastLink == HaInfo::Link::Remote ? "external" : "internal") + ", ok " +
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
    : saved_(saved), edit_(saved), layout_(computeSettingsLayout(current)), ringVisible_(ringVisible) {
  for (int i = 1; i < 5; ++i) pages_[i] = computePageLayout(static_cast<Page>(i), current);
}

Update SettingsScreen::activateOverview(int stop) {
  Update u;
  if (stop == kStopBack) {
    u.action = Action::Leave;
  } else if (stop == kStopRefresh) {
    if (refresh_ == RefreshState::Running) return u;
    refresh_ = RefreshState::Running;
    u.action = Action::StartRefresh;
    u.dirty.push_back(layout_.refreshButton);
    u.dirty.push_back(layout_.info[static_cast<int>(InfoId::HomeAssistant)].rect);
  } else if (stop >= 0 && stop < kCategoryCount) {
    focus_[0] = stop;
    page_ = categoryPage(stop);
    focus_[static_cast<int>(page_)] = 0;
    u.action = Action::PageChange;
  }
  return u;
}

Update SettingsScreen::up() {
  Update u;
  const int cat = static_cast<int>(page_) - 1;
  page_ = Page::Main;
  focus_[0] = cat;  // the overview focus returns to the category row just left
  u.action = Action::PageChange;
  return u;
}

Update SettingsScreen::activatePage(int stop) {
  Update u;
  const PageLayout& P = pageLayout();
  if (stop == P.rowCount) return up();
  if (stop < 0 || stop > P.rowCount) return u;
  const RowId id = P.rowIds[stop];
  const Rect& row = P.rows[stop];
  const bool wasDirty = dirty();
  const bool hadNote = pageShowsNote(page_, edit_);
  edit_ = cycleRow(id, edit_, net_);
  if (isPipRow(id)) {
    u.dirty.push_back(rowValueRect(row));
    u.dirty.push_back(rowHintRect(row));
  } else {
    u.dirty.push_back(hintRowValueRect(row));
  }
  if (hadNote != pageShowsNote(page_, edit_)) u.dirty.push_back(P.note);
  if (wasDirty != dirty()) u.dirty.push_back(P.unsavedChip);
  return u;
}

Update SettingsScreen::onTouch(Point p) {
  const int n = stopCount();
  for (int i = 0; i < n; ++i) {
    const Rect& r = page_ == Page::Main ? layout_.stops[i] : pageLayout().stops[i];
    if (r.contains(p)) return page_ == Page::Main ? activateOverview(i) : activatePage(i);
  }
  return {};
}

Update SettingsScreen::onKey(ui::LogicalKey key) {
  Update u;
  const int n = stopCount();
  switch (key) {
    case ui::LogicalKey::Prev:
    case ui::LogicalKey::Next: {
      int& f = focus_[static_cast<int>(page_)];
      const Rect* stops = page_ == Page::Main ? layout_.stops : pageLayout().stops;
      const int old = f;
      f = key == ui::LogicalKey::Next ? (f + 1) % n : (f + n - 1) % n;
      if (ringVisible_) u.dirty.push_back(ui::inflate(stops[old], kRingInflate));
      u.dirty.push_back(ui::inflate(stops[f], kRingInflate));
      ringVisible_ = true;
      break;
    }
    case ui::LogicalKey::Push:
      u = page_ == Page::Main ? activateOverview(focus_[0]) : activatePage(focus());
      break;
    case ui::LogicalKey::LongPush:
      if (page_ == Page::Main) u.action = Action::Leave;  // overview: leave Settings
      else u = up();                                       // category page: one level up
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
  r.networkChanged = edit_.wifi_profile != saved_.wifi_profile || edit_.ha_link != saved_.ha_link;
  r.settings = edit_;
  return r;
}

}  // namespace settings_ui
