#pragma once
// Sleep screen (task 101, docs/ui/sleep-screen.md): the frozen last frame before sleep when Settings > While asleep
// is `Sleep screen`. Pure logic: block selection, text, fallbacks and the layout rects. The drawing is
// src/render/sleep_screen_draw.cpp. All geometry is the spec's (section 2), in panel coordinates.
#include <cstdint>
#include <string>
#include <vector>

#include "entity_state.h"
#include "settings.h"

namespace sleepscreen {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool operator==(const Rect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

enum class Cond { Sunny, ClearNight, PartlyCloudy, Cloudy, Rainy, Pouring, Snowy, Fog, Windy, Lightning, Unknown };

// Everything the screen needs, collected by the caller at sleep entry.
struct Inputs {
  bool landscape = false;
  int64_t wakeEpoch = 0;        // clock at this wake (0 / implausible = unknown)
  int32_t utcOffsetS = 0;       // local offset at that time
  int64_t updatedEpoch = 0;     // last successful state fetch (0 = never)
  int64_t nextUpdateEpoch = 0;  // next scheduled wake (0 = no scheduled updates)
  int batteryPct = 0;
  int batteryMv = 0;
  bool charging = false;
  bool haveClimate = false;  // SHT30 reading of this wake
  int tempX10 = 0;
  int humX10 = 0;
  settings::SleepSetting sleepMode = settings::SleepSetting::DeepSleep;
  // Optional blocks (config `sleep_screen:`); the entities are looked up by the caller (null = not in the store).
  bool weatherConfigured = false;
  const EntityState* weather = nullptr;
  bool calendarConfigured = false;
  const EntityState* calendar = nullptr;
  bool notificationsConfigured = false;
  int notificationCount = 0;  // 0 or unknown: block not drawn
  std::string notificationTitle;
};

// Block ids as in the spec (w-N).
enum BlockId { kDate = 1, kUpdated = 2, kBattery = 3, kWeather = 4, kEvent = 5, kNotifications = 6, kFooter = 7 };

struct Placement {
  int id = 0;
  Rect rect;  // the block rect (x, y, W, h)
};

struct Layout {
  std::vector<Placement> blocks;  // w-1..w-6 shown, in id order; the footer is separate
  Rect footerRule;                // 1 px hairline above the footer
  Rect divider;                   // landscape: vertical hairline (w = 0 in portrait)
  Rect footer;                    // footer block rect
  int climateColumnDx = 0;        // x offset of w-3 column 2 (Indoor)
  const Placement* find(int id) const {
    for (const Placement& p : blocks)
      if (p.id == id) return &p;
    return nullptr;
  }
};

// Which optional blocks are drawn (weather, event, notifications) decides the layout.
Layout layoutFor(bool landscape, bool showWeather, bool showEvent, bool showNotifications);

struct DateText {
  std::string weekday, date;  // `Tuesday`, `3 November`
};
struct UpdatedText {
  std::string line1;       // `updated 14:05`, `updated 26.10. 14:05`, `updated never`
  bool small = false;      // F24 instead of F40 (an earlier day)
  std::string line2;       // `next update 15:30` / `no scheduled updates`
};
struct BatteryText {
  int pct = 0;          // 0..100 (level fill)
  bool low = false;     // <= 15: `!` instead of the fill
  std::string percent;  // `78%`
  std::string sub;      // `3.92 V` / `charging`
};
struct ClimateText {
  bool have = false;
  std::string temp, tempUnit;  // `21.5`, degree sign + `C` (or `-` and empty unit)
  std::string hum, humUnit;    // `48`, `%` (or `-` and empty unit)
};
struct WeatherText {
  bool unavailable = false;
  Cond cond = Cond::Unknown;
  std::string temp, unit;  // `12`, degree sign + `C`; `-` and empty without a value
  std::string condition;   // `Partly cloudy`, `n/a`
};
enum class EventKind { Timed, AllDay, None, Unavailable };
struct EventText {
  EventKind kind = EventKind::None;
  std::string time;   // `16:30` (Timed), `all day` (AllDay), `Nothing planned` (None), `Calendar n/a` (Unavailable)
  std::string title;  // Timed / AllDay
  std::string day;    // `Today` / `Tomorrow` / weekday / `3 Nov`; empty = no day line
};
struct NotificationsText {
  std::string count;  // `3`
  std::string title;
};

struct View {
  bool landscape = false;
  bool showWeather = false, showEvent = false, showNotifications = false;
  Layout layout;
  DateText date;
  UpdatedText updated;
  BatteryText battery;
  ClimateText climate;
  WeatherText weather;
  EventText event;
  NotificationsText notifications;
  std::string footerLabel;  // `Asleep`
  std::string footerHint;   // by Sleep mode
};

View compose(const Inputs& in);

// ---- pieces (exposed for tests) ----
DateText dateText(int64_t epoch, int32_t utcOffsetS);  // epoch 0 / implausible: empty strings
std::string wakeHint(settings::SleepSetting m);
std::string conditionText(const std::string& haState);
Cond conditionOf(const std::string& haState);
// "2026-11-03 16:30:00" / "2026-11-03" (local, as HA's calendar attribute); false for anything else.
struct LocalTime {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0;
  bool hasTime = false;
};
bool parseLocalTime(const std::string& s, LocalTime& out);
int weekdayOf(int year, int month, int day);  // 0 = Sunday

}  // namespace sleepscreen
