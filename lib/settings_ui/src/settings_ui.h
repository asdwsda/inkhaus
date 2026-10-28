#pragma once
// Settings screen logic and layout (docs/ui/settings.md). Pure, no drawing, no Arduino.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "settings.h"
#include "ui_layout.h"
#include "ui_rocker.h"

namespace settings_ui {

using ui::Point;
using ui::Rect;

// ---- layout (spec sections 3 and 4) ----
constexpr int kRowCount = 4;      // Orientation, Wake mode, Refresh every, Idle timeout
constexpr int kInfoRowCount = 6;  // Firmware, Battery, HA, Wi-Fi, Config, Errors
constexpr int kFocusStops = 6;    // 4 rows, Refresh now, Back
constexpr int kStopRefresh = 4;   // 0-based focus stop indices
constexpr int kStopBack = 5;
constexpr int kRingInflate = 6;

enum class InfoId { Firmware, Battery, HomeAssistant, WiFi, Config, Errors };

struct InfoRow {
  Rect rect;                     // whole row, absolute
  Rect hairline;                 // 1 px top edge
  int labelX = 0;                // absolute x of the label (left aligned)
  int valueX = 0;                // absolute x of the value without icon (left aligned)
  int iconX = 0;                 // absolute x of the 24x24 status icon; value then at iconX + 32
  int cy = 0;                    // absolute centre line (single line rows)
  int cyLine1 = 0, cyLine2 = 0;  // Config row only
  int valueWidth = 0;            // width available for the value without icon
};

struct SettingsLayout {
  settings::Orientation orientation = settings::Orientation::Portrait;
  int screenW = 0, screenH = 0;
  Rect title;  // text box, F40, vertically centred in the box
  Rect unsavedChip;
  Rect headerRule;
  Rect rows[kRowCount];
  Rect note;
  InfoRow info[kInfoRowCount];
  Rect refreshButton, backButton;
  Rect stops[kFocusStops];  // focus order; same rects as rows + buttons
};
SettingsLayout computeSettingsLayout(settings::Orientation o);

// Inside one setting row; all results absolute.
Rect rowLabelRect(const Rect& row);   // row-relative (16,18,220,24), cy 30
Rect rowHintRect(const Rect& row);    // (16,50,220,24), cy 62
Rect rowValueRect(const Rect& row);   // (240,0,w-256,88), right aligned, cy 44
int rowValueRightX(const Rect& row);  // row.x + row.w - 16
int rowLabelCy(const Rect& row);
int rowHintCy(const Rect& row);
int rowValueCy(const Rect& row);
int pipCount(int rowIndex);  // 2, 2, 6, 4
Point pipCentre(const Rect& row, int k);  // (row.x + 22 + 20k, row.y + 62)
constexpr int kPipDiameter = 12;
Rect rowWarnIconRect(const Rect& row);  // (16,50,24,24) row-relative
int rowWarnTextX(const Rect& row);      // row.x + 46 (cy = rowHintCy)
Rect noteIconRect(const Rect& note);    // (12,12,40,40) slot-relative
int noteTextX(const Rect& note);        // note.x + 64
int noteLineCy(const Rect& note, int line);  // line 0: +21, line 1: +43
Rect buttonIconRect(const Rect& button);     // (12,12,40,40) button-relative
Rect buttonLabelRect(const Rect& button);    // (60,0,w-68,64)

// ---- text ----
const char* rowLabel(int rowIndex);
// "Portrait", "Button", "Off" / "15 min", "20 s".
std::string rowValueText(int rowIndex, const settings::Settings& s);
// Index of the current value in its allowed-values array (-1 if not allowed).
int rowValueIndex(int rowIndex, const settings::Settings& s);
// Next allowed value, wrapping last -> first. An unknown value goes to the first.
settings::Settings cycleRow(int rowIndex, const settings::Settings& s);

constexpr const char* kNoteLine1 = "Touch wake shortens battery life.";
constexpr const char* kNoteLine2 = "Button wake is recommended.";
constexpr const char* kWarnHint = "drains battery";
// True: the hint line shows warn icon + text instead of the pips.
bool showsWakeWarning(int rowIndex, const settings::Settings& s);
// True: the note slot is drawn (wake mode = Touch).
bool showsNote(const settings::Settings& s);

// Text fit rule: F24, then F18, then an ellipsis. measure(text, fontPx) gives the width.
struct FittedText {
  std::string text;
  int font = 24;  // 24 or 18
};
FittedText fitText(const std::string& text, int maxWidth,
                   const std::function<int(const std::string&, int)>& measure);

// ---- info rows (read-only values) ----
enum class Icon { None, Alert, Warn, Bolt, Hourglass };

struct WallTime {
  int month = 0, day = 0, hour = 0, minute = 0;
};
// Same day "14:05", otherwise "26.10. 13:05".
std::string formatTime(const WallTime& t, const WallTime& now);

struct BatteryInfo {
  int percent = 0;
  int millivolts = 0;
  bool charging = false;
};
struct HaInfo {
  enum class Link { None, Local, Remote };
  bool sessionDone = false;  // false before the first session: "-"
  Link lastLink = Link::None;
  bool lastAttemptOk = false;
  bool everOk = false;
  WallTime lastOk;
  bool refreshing = false;
};
struct WifiInfo {
  bool configured = false;
  std::string ssid;
  bool connected = false;
};
struct ConfigInfo {
  bool missing = false;
  int errorCount = 0;
  std::string firstPath;  // e.g. tabs[0].widgets[2].entity
  int firstLine = 0;
};

struct InfoText {
  Icon icon = Icon::None;
  std::string text;
};
constexpr int kLowBatteryPercent = 15;
InfoText batteryText(const BatteryInfo& b);  // "78%, 3.92 V"; alert if <= 15, bolt if charging
InfoText haText(const HaInfo& h, const WallTime& now);
InfoText wifiText(const WifiInfo& w);
InfoText configText(const ConfigInfo& c);   // "OK" / alert + "path (line N)" / warn + "no config.yaml on SD"
std::string errorsText(const ConfigInfo& c);  // "0", "3"; "-" when missing

// ---- screen state machine (spec sections 2, 5, 6) ----
constexpr uint32_t kIdleLeaveMs = 60000;  // fixed while Settings is open (rule 5, OPEN-S1)

enum class RefreshState { Idle, Running, Failed };

enum class Action {
  None,
  Leave,         // caller calls leave()
  StartRefresh,  // caller starts the state fetch, later calls refreshDone()
};

struct Update {
  Action action = Action::None;
  std::vector<Rect> dirty;  // partial redraw regions (absolute; ring regions already inflated)
};

struct LeaveResult {
  bool save = false;                // buffer != saved: call settings::save(settings) once
  bool orientationChanged = false;  // rotate, recompute layout, one full refresh
  settings::Settings settings;      // the edit buffer
  bool fullRefresh = true;          // leaving is always a full refresh
};

class SettingsScreen {
 public:
  // The layout is for orientation `current` (the one in force while Settings is open).
  // ringVisible: true when opened by the rocker.
  SettingsScreen(const settings::Settings& saved, settings::Orientation current,
                 bool ringVisible);

  const settings::Settings& edit() const { return edit_; }
  const settings::Settings& saved() const { return saved_; }
  bool dirty() const { return edit_ != saved_; }
  const SettingsLayout& layout() const { return layout_; }
  int focus() const { return focus_; }  // 0-based stop
  bool ringVisible() const { return ringVisible_; }
  RefreshState refreshState() const { return refresh_; }
  Rect focusRect() const { return layout_.stops[focus_]; }

  Update onTouch(Point p);
  Update onKey(ui::LogicalKey key);
  // Fetch finished. On success the Battery and HA rows are redrawn too.
  Update refreshDone(bool ok);
  static bool idleExpired(uint32_t idleMs) { return idleMs >= kIdleLeaveMs; }
  LeaveResult leave() const;

 private:
  Update activate(int stop);

  settings::Settings saved_, edit_;
  SettingsLayout layout_;
  int focus_ = 0;
  bool ringVisible_ = false;
  RefreshState refresh_ = RefreshState::Idle;
};

}  // namespace settings_ui
