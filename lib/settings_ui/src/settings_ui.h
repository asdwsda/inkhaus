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

// ---- structure (spec section 3, revision 7) ----
// Overview (Page::Main) with four category rows, and one page per category.
enum class Page { Main, Display, Sleep, Network, Tools };
constexpr int kCategoryCount = 4;
inline Page categoryPage(int i) { return static_cast<Page>(i + 1); }
const char* categoryLabel(int i);  // "Display", "Sleep & wake", "Network", "Tools" (also the page title)

// Setting rows, in the edit buffer order of the categories.
enum class RowId { Orientation, Wake, Refresh, Idle, Wifi, HaLink, Demo, Debug };
constexpr int kRowIdCount = 8;
constexpr int kMaxPageRows = 3;
bool isPipRow(RowId r);  // Orientation, Wake, Refresh, Idle (2..6 fixed values); the others are hint rows (88 high)

// ---- layout (spec sections 3 and 8) ----
constexpr int kInfoRowCount = 6;  // Firmware, Battery, HA, Wi-Fi, Config, Errors
constexpr int kOverviewStops = 6; // 4 category rows, Refresh now, Back
constexpr int kStopRefresh = 4;   // 0-based focus stop indices (overview)
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

// Overview.
struct SettingsLayout {
  settings::Orientation orientation = settings::Orientation::Portrait;
  int screenW = 0, screenH = 0;
  Rect title;  // text box, F40, vertically centred in the box
  Rect unsavedChip;
  Rect headerRule;
  Rect categories[kCategoryCount];
  InfoRow info[kInfoRowCount];
  Rect refreshButton, backButton;
  Rect stops[kOverviewStops];  // focus order; same rects as categories + buttons
};
SettingsLayout computeSettingsLayout(settings::Orientation o);

// A category page (spec section 8). Focus stops = rows top to bottom (landscape Network/Tools: left to
// right), then Back.
struct PageLayout {
  Page page = Page::Display;
  settings::Orientation orientation = settings::Orientation::Portrait;
  int screenW = 0, screenH = 0;
  Rect title, unsavedChip, headerRule;
  int rowCount = 0;
  RowId rowIds[kMaxPageRows];
  Rect rows[kMaxPageRows];
  Rect note;     // w == 0: the page has no note slot
  Rect caption;  // Display only (w == 0 elsewhere): text box of the static caption, F18 DIM, cy = y + h / 2
  Rect backButton;
  int stopCount = 0;  // rowCount + 1
  Rect stops[kMaxPageRows + 1];
};
PageLayout computePageLayout(Page p, settings::Orientation o);
constexpr const char* kDisplayCaption = "Applied when Settings is left.";

// Inside one setting row; all results absolute.
// Main page rows are 72 (portrait) or 68 (landscape) high: label cy = h/2 - 10, hint cy = h/2 + 13,
// value cy = h/2 (portrait 26 / 49 / 36, landscape 24 / 47 / 34).
Rect rowLabelRect(const Rect& row);   // (16, cyL-14, 220, 28)
Rect rowHintRect(const Rect& row);    // (16, cyH-9, 220, 18)
Rect rowValueRect(const Rect& row);   // (240,0,w-256,h), right aligned
int rowValueRightX(const Rect& row);  // row.x + row.w - 16
int rowLabelCy(const Rect& row);
int rowHintCy(const Rect& row);
int rowValueCy(const Rect& row);
// Value font decision: 0 = F40 fits, 1 = F24 fits, 2 = neither (truncate F24 with "...").
constexpr int kValueGap = 16;
inline int valueFitLevel(int w40, int w24, int freeW) { return w40 <= freeW ? 0 : (w24 <= freeW ? 1 : 2); }
Point pipCentre(const Rect& row, int k);  // (row.x + 22 + 20k, hint cy)
constexpr int kPipDiameter = 12;
Rect noteIconRect(const Rect& note);    // (12,12,40,40) slot-relative
int noteTextX(const Rect& note);        // note.x + 64
int noteLineCy(const Rect& note, int line);  // line 0: +21, line 1: +43
Rect buttonIconRect(const Rect& button);     // (12,12,40,40) button-relative
Rect buttonLabelRect(const Rect& button);    // (60,0,w-68,64)

// Category row of the overview (row-relative 508 x 96 / 456 x 96, spec 3.0): label F24 at x + 16, cy + 34;
// one-line summary F18 at x + 16, cy + 66; chevron (40x40) at (W - 52, 28).
int categoryLabelX(const Rect& r);
int categoryLabelCy(const Rect& r);
int categorySummaryCy(const Rect& r);
int categorySummaryMaxWidth(const Rect& r);  // W - 32 - 52
Rect categoryChevronRect(const Rect& r);

// Hint rows (88 high): label cy +32 (F24), hint cy +62 (F18), value cy +44 (F40, right edge W-16).
int hintRowLabelCy(const Rect& r);
int hintRowHintCy(const Rect& r);
int hintRowValueCy(const Rect& r);
Rect hintRowValueRect(const Rect& r);   // dirty region of a changed value (right 268 px)
int hintRowValueWidth();                // 252: box 240..492 for profile names (fit rule F40, F24, "...")
const char* hintRowLabel(RowId r);      // "Wi-Fi", "HA connection", "Demo mode", "Debug log"
const char* hintRowStaticHint(RowId r); // Demo / Debug hints; "" for the live ones (Wi-Fi, HA connection)

// ---- network info (spec 8.3): a snapshot taken when Settings opens ----
enum class LinkState { Unknown, None, Internal, External };  // the link in use (None = offline)
struct NetworkInfo {
  std::vector<std::string> profiles;  // profile names from secrets.yaml, list order (Wi-Fi cycling)
  std::string connected;              // name of the connected profile, "" = not connected
  LinkState link = LinkState::Unknown;
  bool profileDecides = false;        // the connected profile fixes the link (ha_url != auto)
  bool demo = false;                  // saved demo mode: no network in use
};

// ---- text ----
const char* rowLabel(RowId r);
// "Portrait", "Button", "Off" / "15 min", "20 s", Wi-Fi "Auto" / profile name, "Internal", "On".
std::string rowValueText(RowId r, const settings::Settings& s, const NetworkInfo& n);
// Index of the current value in its allowed-values array (-1 if not allowed / not a pip row).
int rowValueIndex(RowId r, const settings::Settings& s);
int pipCount(RowId r);  // 2, 3, 6, 4 for pip rows, else 0
// Next allowed value, wrapping last -> first. An unknown value goes to the first. Wi-Fi: Auto -> 1..N -> Auto.
settings::Settings cycleRow(RowId r, const settings::Settings& s, const NetworkInfo& n);

// Hints of the Network rows (live snapshot): "Connected: Home", "Not connected", "Link: external (profile)",
// "Link: none (offline)", "Not used (demo mode)".
std::string wifiHint(const NetworkInfo& n);
std::string haLinkHint(const settings::Settings& edit, const NetworkInfo& n);

// Overview summaries from the edit buffer (spec 3.0): "Portrait", "Touch, 60 min, 20 s",
// "Wi-Fi Auto, HA Auto", "Demo Off, Log Off". shortWake: "Touch & Button" -> "Both" (when it does not fit).
std::string categorySummary(int category, const settings::Settings& s, const NetworkInfo& n, bool shortWake = false);

// Note slot of a page (spec 8): visible? and its two lines.
struct NoteText {
  const char* line1;
  const char* line2;
};
bool pageShowsNote(Page p, const settings::Settings& s);
NoteText pageNote(Page p, bool sdPresent);
constexpr const char* kNoteLine1 = "Touch wake shortens battery life.";
constexpr const char* kNoteLine2 = "Button wake is recommended.";
constexpr const char* kNetNoteLine1 = "Forced profile: other Wi-Fi";
constexpr const char* kNetNoteLine2 = "networks are not tried.";
constexpr const char* kToolsNoteLine1 = "Writes /debug.log (max 1 MB).";
constexpr const char* kToolsNoteLine2 = "Uses more battery.";
constexpr const char* kToolsNoCardLine1 = "No SD card: nothing is logged.";
constexpr const char* kToolsNoCardLine2 = "Insert a card to log.";

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
  enum class Link { None, Local, Remote };  // Local = internal URL, Remote = external URL (shown so)
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

// Saved demo mode On: the info rows read these (spec section 6); the error count shows "0".
constexpr const char* kDemoHaText = "Demo mode";
constexpr const char* kDemoWifiText = "Demo mode";
constexpr const char* kDemoConfigText = "built-in demo";

// ---- screen state machine (spec sections 2, 5, 6) ----
constexpr uint32_t kIdleLeaveMs = 60000;  // fixed while Settings is open (rule 5, OPEN-S1)

enum class RefreshState { Idle, Running, Failed };

enum class Action {
  None,
  Leave,         // caller calls leave()
  PageChange,    // overview <-> category page: the caller draws the new page with a full (GC16) refresh
  StartRefresh,  // caller starts the state fetch, later calls refreshDone()
};

struct Update {
  Action action = Action::None;
  std::vector<Rect> dirty;  // partial redraw regions (absolute; ring regions already inflated)
};

struct LeaveResult {
  bool save = false;                // buffer != saved: call settings::save(settings) once
  bool orientationChanged = false;  // rotate, recompute layout, one full refresh
  bool demoChanged = false;         // config source changes: reload the config, full redraw
  bool debugLogChanged = false;     // SD logging starts/stops (used from the moment Settings is left)
  bool networkChanged = false;      // Wi-Fi profile or HA connection changed: net::configure + new session
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
  Page page() const { return page_; }
  const PageLayout& pageLayout() const { return pages_[static_cast<int>(page_)]; }
  // Snapshot for the Network page (profile names, hints). Set once when Settings opens.
  void setNetwork(const NetworkInfo& n) { net_ = n; }
  const NetworkInfo& network() const { return net_; }
  // The SD card state, set by the caller when the Tools page opens (drives the Debug log note).
  void setSdPresent(bool present) { sdPresent_ = present; }
  bool sdPresent() const { return sdPresent_; }
  int focus() const { return focus_[static_cast<int>(page_)]; }  // 0-based stop of the current page
  int stopCount() const { return page_ == Page::Main ? kOverviewStops : pageLayout().stopCount; }
  bool ringVisible() const { return ringVisible_; }
  RefreshState refreshState() const { return refresh_; }
  Rect focusRect() const { return page_ == Page::Main ? layout_.stops[focus()] : pageLayout().stops[focus()]; }

  Update onTouch(Point p);
  Update onKey(ui::LogicalKey key);
  // Fetch finished. On success the Battery and HA rows are redrawn too.
  Update refreshDone(bool ok);
  static bool idleExpired(uint32_t idleMs) { return idleMs >= kIdleLeaveMs; }
  LeaveResult leave() const;

 private:
  Update activateOverview(int stop);
  Update activatePage(int stop);
  Update up();  // category page -> overview (no save, no apply)

  settings::Settings saved_, edit_;
  SettingsLayout layout_;
  PageLayout pages_[5];  // indexed by Page (Main unused)
  NetworkInfo net_;
  Page page_ = Page::Main;
  int focus_[5] = {0, 0, 0, 0, 0};
  bool sdPresent_ = true;
  bool ringVisible_ = false;
  RefreshState refresh_ = RefreshState::Idle;
};

}  // namespace settings_ui
