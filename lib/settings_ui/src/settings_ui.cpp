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

bool isPipRow(RowId r) { return r == RowId::Orientation || r == RowId::Updates || r == RowId::Idle; }

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
  L.stops[kStopSleepNow] = L.sleepNowButton;
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
    L.sleepNowButton = {16, 804, 508, 64};
    L.refreshButton = {16, 880, 248, 64};
    L.backButton = {276, 880, 248, 64};
    const int ys[kInfoRowCount] = {504, 548, 592, 636, 680, 744};
    const int hs[kInfoRowCount] = {44, 44, 44, 44, 64, 48};
    fillInfo(L, 16, 508, ys, hs);
  } else {
    L.screenW = 960;
    L.screenH = 540;
    L.title = {16, 0, 200, 50};
    L.unsavedChip = {824, 9, 120, 32};
    L.headerRule = {0, 50, 960, 2};
    for (int i = 0; i < kCategoryCount; ++i) L.categories[i] = {16, 64 + 108 * i, 456, 96};
    L.sleepNowButton = {488, 388, 456, 64};
    L.refreshButton = {488, 464, 222, 64};
    L.backButton = {722, 464, 222, 64};
    const int ys[kInfoRowCount] = {64, 112, 160, 208, 256, 328};
    const int hs[kInfoRowCount] = {48, 48, 48, 48, 72, 48};
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
      // Spec 8.2: Sleep mode, Updates, Keep awake, Idle, Learn; notes stacked (pitch 76); estimate block 124.
      L.rowCount = 5;
      L.rowIds[0] = RowId::SleepMode;
      L.rowIds[1] = RowId::Updates;
      L.rowIds[2] = RowId::KeepAwake;
      L.rowIds[3] = RowId::Idle;
      L.rowIds[4] = RowId::LearnBattery;
      if (portrait) {
        L.rows[0] = {16, 68, 508, 88};
        L.rows[1] = {16, 168, 508, 72};
        L.rows[2] = {16, 252, 508, 88};
        L.rows[3] = {16, 352, 508, 72};
        L.rows[4] = {16, 436, 508, 88};
        L.note = {16, 536, 508, 64};
        L.note2 = {16, 612, 508, 64};
        L.estimate = {16, 744, 508, 124};
      } else {
        L.rows[0] = {16, 64, 456, 88};
        L.rows[1] = {16, 164, 456, 68};
        L.rows[2] = {16, 244, 456, 88};
        L.rows[3] = {16, 344, 456, 68};
        L.rows[4] = {488, 64, 456, 88};
        L.note = {488, 164, 456, 64};
        L.note2 = {488, 240, 456, 64};
        L.estimate = {488, 328, 456, 124};
      }
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
      // Task 087: Firmware is the third row; the Debug log note moves under it (portrait) / stays (landscape).
      L.rowCount = 3;
      L.rowIds[0] = RowId::Demo;
      L.rowIds[1] = RowId::Debug;
      L.rowIds[2] = RowId::Firmware;
      if (portrait) {
        L.rows[0] = {16, 68, 508, 88};
        L.rows[1] = {16, 168, 508, 88};
        L.rows[2] = {16, 268, 508, 88};
        L.note = {16, 368, 508, 64};
      } else {
        L.rows[0] = {16, 64, 456, 88};
        L.rows[1] = {488, 64, 456, 88};
        L.rows[2] = {16, 164, 456, 88};
        L.note = {488, 164, 456, 64};
      }
      break;
    case Page::Confirm: {
      const ConfirmLayout C = computeConfirmLayout(o);
      L.rowCount = 0;
      for (int i = 0; i < 2; ++i) L.stops[i] = C.buttons[i];
      L.stopCount = 2;
      L.backButton = {0, 0, 0, 0};
      return L;
    }
    case Page::Main:
      break;
  }
  for (int i = 0; i < L.rowCount; ++i) L.stops[i] = L.rows[i];
  L.stops[L.rowCount] = L.backButton;
  L.stopCount = L.rowCount + 1;
  return L;
}

ConfirmLayout computeConfirmLayout(settings::Orientation o) {
  ConfirmLayout C;
  if (o == settings::Orientation::Portrait) {
    C.title = {16, 100 - 27, 508, 54};
    for (int i = 0; i < 3; ++i) C.infoRows[i] = {16, 144 + 48 * i, 508, 48};
    const int ly[3] = {330, 354, 378};
    for (int i = 0; i < 3; ++i) C.lineCy[i] = ly[i];
    C.note = {16, 410, 508, 64};
    for (int i = 0; i < 2; ++i) C.buttons[i] = {16, 696 + 76 * i, 508, 64};
  } else {
    C.title = {16, 90 - 27, 456, 54};
    for (int i = 0; i < 3; ++i) C.infoRows[i] = {16, 124 + 44 * i, 456, 44};
    const int ly[3] = {282, 306, 330};
    for (int i = 0; i < 3; ++i) C.lineCy[i] = ly[i];
    C.note = {16, 364, 456, 64};
    for (int i = 0; i < 2; ++i) C.buttons[i] = {488, 200 + 76 * i, 456, 64};
  }
  return C;
}

std::string formatSize(uint32_t bytes) {
  char buf[24];
  if (bytes == 0) return "-";
  if (bytes >= 1000 * 1000) std::snprintf(buf, sizeof buf, "%.1f MB", bytes / 1048576.0);
  else std::snprintf(buf, sizeof buf, "%u KB", (unsigned)((bytes + 512) / 1024));
  return buf;
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
    case RowId::SleepMode: return "Sleep mode";
    case RowId::KeepAwake: return "Keep awake";
    case RowId::LearnBattery: return "Learn battery limits";
    case RowId::Firmware: return "Firmware";
    default: return "";
  }
}
const char* hintRowStaticHint(RowId r) {
  switch (r) {
    case RowId::Demo: return "Sample dashboard, offline";
    case RowId::Debug: return "Log lines to SD card";
    case RowId::LearnBattery: return "Raise low limit after brownouts";
    default: return "";
  }
}
std::string sleepRowHint(RowId r, const Settings& s, const settings::Capabilities& c) {
  if (r == RowId::SleepMode) {
    switch (s.sleep_mode) {
      case settings::SleepSetting::PowerOff: return "Lowest drain, hold 3 s";
      case settings::SleepSetting::DeepSleep: return "Rocker, faster wake";
      case settings::SleepSetting::Touch: return "Tap wakes";
      case settings::SleepSetting::TouchAndRocker: return "Tap or rocker";
    }
  }
  if (r == RowId::KeepAwake) {
    const bool offered = settings::keepAwakeOnUsbOffered(s.sleep_mode, c);
    switch (s.keep_awake) {
      case settings::KeepAwake::Always: return "Never sleeps";
      case settings::KeepAwake::OnUsb: return offered ? "Awake while on USB" : "On USB only in Power off";
      default: return offered ? "Sleeps as set above" : "On USB only in Power off";
    }
  }
  return "";
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
    case RowId::Updates: return "Updates while asleep";
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
  // A name that is not in the list (profile removed or renamed) counts as Auto.
  return settings::wifiProfileIndex(s.wifi_profile, n.profiles) == 0 ? "Auto" : s.wifi_profile;
}
}  // namespace

int pipCount(RowId r) {
  switch (r) {
    case RowId::Orientation: return static_cast<int>(settings::kOrientationValues.size());
    case RowId::Updates: return static_cast<int>(settings::kRefreshIntervalMinValues.size());
    case RowId::Idle: return static_cast<int>(settings::kIdleTimeoutSValues.size());
    default: return 0;
  }
}

int rowValueIndex(RowId r, const Settings& s) {
  switch (r) {
    case RowId::Orientation: return indexOf(settings::kOrientationValues, s.orientation);
    case RowId::SleepMode: return indexOf(settings::kSleepModeValues, s.sleep_mode);
    case RowId::KeepAwake: return indexOf(settings::kKeepAwakeValues, s.keep_awake);
    case RowId::Updates: return indexOf(settings::kRefreshIntervalMinValues, s.refresh_interval_min);
    case RowId::Idle: return indexOf(settings::kIdleTimeoutSValues, s.idle_timeout_s);
    default: return -1;
  }
}

std::string rowValueText(RowId r, const Settings& s, const NetworkInfo& n) {
  char buf[24];
  switch (r) {
    case RowId::Orientation: return s.orientation == settings::Orientation::Landscape ? "Landscape" : "Portrait";
    case RowId::SleepMode:
      return s.sleep_mode == settings::SleepSetting::PowerOff      ? "Power off"
             : s.sleep_mode == settings::SleepSetting::DeepSleep   ? "Deep sleep"
             : s.sleep_mode == settings::SleepSetting::Touch       ? "Touch"
                                                                : "Touch & rocker";
    case RowId::Updates:
      if (s.refresh_interval_min == 0 || settings::updatesForcedOff(s)) return "Off";
      if (s.refresh_interval_min < 60) std::snprintf(buf, sizeof buf, "%d min", static_cast<int>(s.refresh_interval_min));
      else std::snprintf(buf, sizeof buf, "%d h", static_cast<int>(s.refresh_interval_min / 60));
      return buf;
    case RowId::KeepAwake:
      return s.keep_awake == settings::KeepAwake::Always ? "Always" : s.keep_awake == settings::KeepAwake::OnUsb ? "On USB" : "Off";
    case RowId::Idle:
      std::snprintf(buf, sizeof buf, "%d s", static_cast<int>(s.idle_timeout_s));
      return buf;
    case RowId::Wifi: return wifiValue(s, n);
    case RowId::HaLink: return haLinkName(s.ha_link);
    case RowId::Demo: return s.demo_mode ? "On" : "Off";
    case RowId::Debug: return s.debug_log ? "On" : "Off";
    case RowId::LearnBattery: return s.learn_battery ? "On" : "Off";
    case RowId::Firmware: return "";
  }
  return "";
}

Settings cycleRow(RowId r, const Settings& s, const NetworkInfo& n, const settings::Capabilities& caps) {
  Settings o = s;
  const int cur = rowValueIndex(r, s);
  switch (r) {
    case RowId::Orientation: o.orientation = settings::kOrientationValues[nextIndex(settings::kOrientationValues, cur)]; break;
    case RowId::SleepMode:
      o.sleep_mode = settings::kSleepModeValues[nextIndex(settings::kSleepModeValues, cur)];
      o = settings::normalizeSleepFields(o, caps);  // On USB drops to Off where the mode does not offer it
      break;
    case RowId::KeepAwake:
      o.keep_awake = settings::kKeepAwakeValues[nextIndex(settings::kKeepAwakeValues, cur)];
      if (o.keep_awake == settings::KeepAwake::OnUsb && !settings::keepAwakeOnUsbOffered(o.sleep_mode, caps))
        o.keep_awake = settings::KeepAwake::Always;
      break;
    case RowId::Updates:
      o.refresh_interval_min =
          settings::kRefreshIntervalMinValues[nextIndex(settings::kRefreshIntervalMinValues, cur)];
      break;
    case RowId::Idle:
      o.idle_timeout_s = settings::kIdleTimeoutSValues[nextIndex(settings::kIdleTimeoutSValues, cur)];
      break;
    case RowId::Wifi: {
      const int cur = settings::wifiProfileIndex(s.wifi_profile, n.profiles);  // 0 = Auto / unknown
      o.wifi_profile = cur >= static_cast<int>(n.profiles.size()) ? std::string() : n.profiles[static_cast<size_t>(cur)];
      break;
    }
    case RowId::HaLink:
      o.ha_link = s.ha_link == settings::HaLink::Auto       ? settings::HaLink::Internal
                  : s.ha_link == settings::HaLink::Internal ? settings::HaLink::External
                                                            : settings::HaLink::Auto;
      break;
    case RowId::Demo: o.demo_mode = !s.demo_mode; break;
    case RowId::Debug: o.debug_log = !s.debug_log; break;
    case RowId::LearnBattery: o.learn_battery = !s.learn_battery; break;
    case RowId::Firmware: break;  // not a setting
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

std::string categorySummary(int category, const Settings& s, const NetworkInfo& n, bool shortWake,
                            const settings::Capabilities&, bool updateAvailable) {
  switch (category) {
    case 0: return rowValueText(RowId::Orientation, s, n);
    case 1: {
      std::string mode = rowValueText(RowId::SleepMode, s, n);
      if (shortWake && s.sleep_mode == settings::SleepSetting::TouchAndRocker) mode = "Touch+rocker";
      return mode + ", " + rowValueText(RowId::Updates, s, n) + ", " + rowValueText(RowId::Idle, s, n);
    }
    case 2: return "Wi-Fi " + wifiValue(s, n) + ", HA " + haLinkName(s.ha_link);
    case 3: return std::string(updateAvailable ? "Update available, " : "") + "Demo " + (s.demo_mode ? "On" : "Off") + ", Log " + (s.debug_log ? "On" : "Off");
  }
  return "";
}

bool pageShowsNote(Page p, const Settings& s, const NetworkInfo& n) {
  switch (p) {
    case Page::Sleep: return sleepNotes(s).count > 0;
    case Page::Network: return settings::wifiProfileIndex(s.wifi_profile, n.profiles) != 0;
    case Page::Tools: return s.debug_log;
    default: return false;
  }
}

SleepNotes sleepNotes(const Settings& s, const settings::Capabilities& c) {
  SleepNotes n;
  if (settings::updatesForcedOff(s, c)) {
    n.note[n.count++] = {kForcedNoteLine1, kForcedNoteLine2};
    n.key |= 1;
  }
  if (s.sleep_mode == settings::SleepSetting::Touch || s.sleep_mode == settings::SleepSetting::TouchAndRocker) {
    n.note[n.count++] = {kNoteLine1, kNoteLine2};
    n.key |= 2;
  }
  return n;
}

EstimateView estimateView(const Settings& s, const settings::Capabilities& c) {
  EstimateView v;
  v.level = power::est::batteryUseLevel(s, c);
  v.text = std::to_string(v.level) + "/10";
  return v;
}

Rect estimateLiveRect(const Rect& b) { return {b.x, b.y + 36, b.w, 88}; }

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
  if (b.learnedLimitMv > 0) {  // task 079: "78%, 3.92 V, min 3.80 V" (UNVERIFIED: fits the value column)
    const int l = (b.learnedLimitMv + 5) / 10;
    std::snprintf(buf, sizeof buf, "%d%%, %d.%02d V, min %d.%02d V", b.percent, v / 100, v % 100, l / 100, l % 100);
  } else {
    std::snprintf(buf, sizeof buf, "%d%%, %d.%02d V", b.percent, v / 100, v % 100);
  }
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

// ---- firmware row ----

namespace {
std::string fwDay(const WallTime& t, const WallTime& now) {
  char buf[16];
  if (t.day == now.day && t.month == now.month) std::snprintf(buf, sizeof buf, "%02d:%02d", t.hour, t.minute);
  else std::snprintf(buf, sizeof buf, "%d.%d.", t.day, t.month);
  return buf;
}
}  // namespace

FirmwareRow firmwareRow(const FirmwareInfo& f) {
  FirmwareRow r;
  r.value = f.current;
  if (f.downloading) {
    r.look = FwLook::Downloading;
    r.dashed = true;
    r.bar = true;
    const std::string v = f.available;
    switch (f.phase) {
      case FwPhase::Downloading:
        r.label = "Downloading " + v;
        r.value = std::to_string(f.pct) + "%";
        break;
      case FwPhase::Verifying:
        r.label = "Checking " + v;
        r.value = "100%";
        break;
      case FwPhase::Restarting:
        r.label = "Restarting";
        r.value = "";
        break;
    }
    return r;
  }
  if (f.checking) {
    r.look = FwLook::Checking;
    r.dashed = true;
    r.hint = "Checking...";
    return r;
  }
  if (!f.rolledBack.empty()) {
    r.look = FwLook::RolledBack;
    r.plain = true;
    r.alert = true;
    r.hint = f.rolledBack + " failed, rolled back";
    return r;
  }
  if (!f.updatedFrom.empty()) {
    r.look = FwLook::Updated;
    r.plain = true;
    r.hint = "Updated from " + f.updatedFrom;
    return r;
  }
  if (!f.available.empty()) {
    r.chip = true;
    const std::string v = f.available;
    if (f.demo || !f.wifi || f.blockedBattery) {
      r.look = FwLook::Refused;
      r.dotted = true;
      r.hint = f.demo ? v + ": leave demo mode first" : !f.wifi ? v + ": Wi-Fi not connected" : v + " needs USB or 50% battery";
      return r;
    }
    r.actionable = true;
    if (f.dlErr != FwDlErr::None) {
      r.look = FwLook::Failed;
      r.alert = true;
      r.hint = f.dlErr == FwDlErr::Damaged ? "File damaged, not installed" : "Download failed, try again";
    } else {
      r.look = FwLook::Available;
      r.hint = "Update to " + v + " available";
    }
    return r;
  }
  r.actionable = true;
  if (f.checkErr != FwCheckErr::None) {
    r.look = FwLook::CheckFailed;
    r.alert = true;
    r.hint = std::string("Check failed: ") + (f.checkErr == FwCheckErr::NoWifi ? "no Wi-Fi" : f.checkErr == FwCheckErr::Timeout ? "timeout" : "no server");
    return r;
  }
  r.look = FwLook::UpToDate;
  if (f.lastCheck.month == 0) r.hint = "Not checked. Tap to check";
  else if (f.justChecked) r.hint = "Up to date, checked " + fwDay(f.lastCheck, f.now);
  else {
    const bool today = f.lastCheck.day == f.now.day && f.lastCheck.month == f.now.month;
    r.hint = "Checked " + fwDay(f.lastCheck, f.now) + (today ? "." : "") + " Tap to check";
  }
  return r;
}

FirmwareRow SettingsScreen::firmwareRow() const {
  FirmwareInfo f = fw_;
  f.demo = f.demo || edit_.demo_mode;
  return settings_ui::firmwareRow(f);
}

// ---- state machine ----

SettingsScreen::SettingsScreen(const Settings& saved, settings::Orientation current,
                               bool ringVisible)
    : saved_(saved), edit_(saved), layout_(computeSettingsLayout(current)), ringVisible_(ringVisible) {
  for (int i = 1; i < kPageCount; ++i) pages_[i] = computePageLayout(static_cast<Page>(i), current);
}

Update SettingsScreen::activateOverview(int stop) {
  Update u;
  if (stop == kStopBack) {
    u.action = Action::Leave;
  } else if (stop == kStopSleepNow) {
    u.action = Action::SleepNow;
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
  if (rowForced(id)) return u;  // forced Off: ignores tap and PUSH
  if (id == RowId::Firmware) return activateFirmware();
  const Rect& row = P.rows[stop];
  const bool wasDirty = dirty();
  const bool hadNote = pageShowsNote(page_, edit_, net_);
  const int oldKey = page_ == Page::Sleep ? sleepNotes(edit_, caps_).key : 0;
  edit_ = cycleRow(id, edit_, net_, caps_);
  if (page_ == Page::Sleep) {
    // Hint rows of this page have live hints; a mode change can flip Updates (forced look) and Keep awake.
    if (id == RowId::SleepMode) {
      for (int i = 0; i < 3; ++i) u.dirty.push_back(P.rows[i]);
    } else {
      u.dirty.push_back(row);
    }
    if (id == RowId::SleepMode || id == RowId::Updates || id == RowId::KeepAwake)
      u.dirty.push_back(estimateLiveRect(P.estimate));
    if (oldKey != sleepNotes(edit_, caps_).key) {
      u.dirty.push_back({P.note.x, P.note.y, P.note.w, P.note2.y + P.note2.h - P.note.y});
    }
  } else {
    if (isPipRow(id)) {
      u.dirty.push_back(rowValueRect(row));
      u.dirty.push_back(rowHintRect(row));
    } else {
      u.dirty.push_back(hintRowValueRect(row));
    }
    if (hadNote != pageShowsNote(page_, edit_, net_)) u.dirty.push_back(P.note);
    if (id == RowId::Demo && page_ == Page::Tools) u.dirty.push_back(P.rows[2]);  // Refused look follows demo mode
  }
  if (wasDirty != dirty()) u.dirty.push_back(P.unsavedChip);
  return u;
}

bool SettingsScreen::rowSkipped(int stop) const {
  if (page_ == Page::Main || page_ == Page::Confirm || stop >= pageLayout().rowCount) return false;
  const RowId id = pageLayout().rowIds[stop];
  return rowForced(id) || (id == RowId::Firmware && !firmwareRow().actionable);
}

Update SettingsScreen::activateFirmware() {
  Update u;
  const FirmwareRow r = firmwareRow();
  if (!r.actionable) return u;
  const Rect& row = pageLayout().rows[2];
  if (r.look == FwLook::UpToDate || r.look == FwLook::CheckFailed) {
    fw_.checking = true;
    fw_.checkErr = FwCheckErr::None;
    fw_.justChecked = false;
    u.action = Action::FirmwareCheck;
    u.dirty.push_back(row);
  } else {  // Available / Failed: the confirm page
    page_ = Page::Confirm;
    focus_[static_cast<int>(Page::Confirm)] = 0;
    ringVisible_ = false;
    u.action = Action::PageChange;
  }
  return u;
}

Update SettingsScreen::activateConfirm(int stop) {
  Update u;
  page_ = Page::Tools;
  u.action = Action::PageChange;
  if (stop == kConfirmYes) {
    fw_.downloading = true;
    fw_.phase = FwPhase::Downloading;
    fw_.pct = 0;
    fw_.dlErr = FwDlErr::None;
    u.action = Action::FirmwareInstall;
    u.dirty.push_back(pageLayout().rows[2]);
  }
  return u;
}

Update SettingsScreen::onTouch(Point p) {
  if (fw_.checking || fw_.downloading) return {};  // all input is ignored until the result
  const int n = stopCount();
  for (int i = 0; i < n; ++i) {
    const Rect& r = page_ == Page::Main ? layout_.stops[i] : pageLayout().stops[i];
    if (r.contains(p))
      return page_ == Page::Main ? activateOverview(i) : page_ == Page::Confirm ? activateConfirm(i) : activatePage(i);
  }
  return {};
}

Update SettingsScreen::onKey(ui::LogicalKey key) {
  Update u;
  if (fw_.checking || fw_.downloading) return u;
  if (page_ == Page::Confirm) {  // rocker yes/no rule: UP yes, DOWN no, PUSH / LONG_PUSH no (no focus ring)
    switch (key) {
      case ui::LogicalKey::Prev: return activateConfirm(kConfirmYes);
      case ui::LogicalKey::Next: return activateConfirm(kConfirmNo);
      default: return activateConfirm(kConfirmNo);
    }
  }
  const int n = stopCount();
  switch (key) {
    case ui::LogicalKey::Prev:
    case ui::LogicalKey::Next: {
      int& f = focus_[static_cast<int>(page_)];
      const Rect* stops = page_ == Page::Main ? layout_.stops : pageLayout().stops;
      const int old = f;
      f = key == ui::LogicalKey::Next ? (f + 1) % n : (f + n - 1) % n;
      if (rowSkipped(f))  // skip a forced / non-actionable row
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
