#include "sleep_screen.h"

#include <cmath>
#include <cstdio>

namespace sleepscreen {

namespace {

constexpr int64_t kMinPlausibleEpoch = 1700000000;  // as app_logic::plausibleEpoch
const char* const kDeg = "\xC2\xB0";

bool plausible(int64_t e) { return e >= kMinPlausibleEpoch; }

struct Civil {
  int year = 1970, month = 1, day = 1, hour = 0, minute = 0;
  int64_t days = 0;  // days since 1970-01-01 (local)
};

// Hinnant's civil_from_days, on local seconds.
Civil civilOf(int64_t epoch, int32_t offsetS) {
  const int64_t t = epoch + offsetS;
  int64_t days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
  const int64_t rem = t - days * 86400;
  Civil c;
  c.days = days;
  c.hour = static_cast<int>(rem / 3600);
  c.minute = static_cast<int>(rem % 3600 / 60);
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const int64_t doe = days - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  c.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  c.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  c.year = static_cast<int>(yoe + era * 400 + (c.month <= 2 ? 1 : 0));
  return c;
}

int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

const char* const kWeekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* const kMonths[12] = {"January", "February", "March",     "April",   "May",      "June",
                                 "July",    "August",   "September", "October", "November", "December"};
const char* const kMonthsShort[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

std::string hhmm(int h, int m) {
  char b[16];
  std::snprintf(b, sizeof b, "%02d:%02d", h, m);
  return b;
}
std::string dayMonth(int d, int m) {
  char b[16];
  std::snprintf(b, sizeof b, "%d.%d.", d, m);
  return b;
}

// Tenths with one decimal, rounded toward the sign: 215 -> "21.5", -5 -> "-0.5".
std::string tenths(int v) {
  const bool neg = v < 0;
  const int a = neg ? -v : v;
  char b[16];
  std::snprintf(b, sizeof b, "%s%d.%d", neg ? "-" : "", a / 10, a % 10);
  return b;
}

std::string roundedNumber(double v) {
  const long r = std::lround(v);
  return std::to_string(r == 0 ? 0 : r);  // no "-0"
}

bool stateUnavailable(const EntityState* st) {
  return !st || !st->available || st->state == "unavailable" || st->state == "unknown";
}

UpdatedText updatedText(const Inputs& in) {
  UpdatedText u;
  const bool clock = plausible(in.wakeEpoch);
  const Civil wake = civilOf(clock ? in.wakeEpoch : 0, in.utcOffsetS);
  if (!plausible(in.updatedEpoch)) {
    u.line1 = "updated never";
  } else {
    const Civil c = civilOf(in.updatedEpoch, in.utcOffsetS);
    if (!clock || c.days == wake.days) {
      u.line1 = "updated " + hhmm(c.hour, c.minute);
    } else {
      u.line1 = "updated " + dayMonth(c.day, c.month) + " " + hhmm(c.hour, c.minute);
      u.small = true;
    }
  }
  if (!plausible(in.nextUpdateEpoch)) {
    u.line2 = "no scheduled updates";
  } else {
    const Civil c = civilOf(in.nextUpdateEpoch, in.utcOffsetS);
    if (!clock || c.days == wake.days) u.line2 = "next update " + hhmm(c.hour, c.minute);
    else u.line2 = "next update " + dayMonth(c.day, c.month) + " " + hhmm(c.hour, c.minute);
  }
  return u;
}

BatteryText batteryText(const Inputs& in) {
  BatteryText b;
  b.pct = in.batteryPct < 0 ? 0 : in.batteryPct > 100 ? 100 : in.batteryPct;
  b.low = b.pct <= 15;
  b.percent = std::to_string(b.pct) + "%";
  if (in.charging) {
    b.sub = "charging";
  } else {
    char s[24];
    std::snprintf(s, sizeof s, "%d.%02d V", in.batteryMv / 1000, in.batteryMv % 1000 / 10);
    b.sub = s;
  }
  return b;
}

ClimateText climateText(const Inputs& in) {
  ClimateText c;
  c.have = in.haveClimate;
  if (!in.haveClimate) {
    c.temp = "-";
    c.hum = "-";
    return c;
  }
  c.temp = tenths(in.tempX10);
  c.tempUnit = std::string(kDeg) + "C";
  const int h = (in.humX10 + 5) / 10;
  c.hum = std::to_string(h < 0 ? 0 : h > 100 ? 100 : h);
  c.humUnit = "%";
  return c;
}

WeatherText weatherText(const EntityState* st) {
  WeatherText w;
  if (stateUnavailable(st)) {
    w.unavailable = true;
    w.temp = "-";
    w.condition = "n/a";
    return w;
  }
  w.cond = conditionOf(st->state);
  w.condition = conditionText(st->state);
  if (st->temperature) {
    w.temp = roundedNumber(*st->temperature);
    w.unit = st->temperature_unit.empty() ? std::string(kDeg) + "C" : st->temperature_unit;
  } else {
    w.temp = "-";
  }
  return w;
}

EventText eventText(const EntityState* st, const Inputs& in) {
  EventText e;
  if (!st || !st->available || st->state == "unavailable") {
    e.kind = EventKind::Unavailable;
    e.time = "Calendar n/a";
    return e;
  }
  const std::string* msg = st->attr("message");
  const std::string* start = st->attr("start_time");
  LocalTime t;
  if (!msg || msg->empty() || !start || !parseLocalTime(*start, t)) {
    e.kind = EventKind::None;
    e.time = "Nothing planned";
    return e;
  }
  e.title = *msg;
  const std::string* allDay = st->attr("all_day");
  const bool all = !t.hasTime || (allDay && *allDay == "true");
  e.kind = all ? EventKind::AllDay : EventKind::Timed;
  e.time = all ? "all day" : hhmm(t.hour, t.minute);
  const int64_t startDay = daysFromCivil(t.year, t.month, t.day);
  if (plausible(in.wakeEpoch)) {
    const int64_t today = civilOf(in.wakeEpoch, in.utcOffsetS).days;
    const int64_t d = startDay - today;
    if (d == 0) e.day = "Today";
    else if (d == 1) e.day = "Tomorrow";
    else if (d > 1 && d < 7) e.day = kWeekdays[weekdayOf(t.year, t.month, t.day)];
    else e.day = std::to_string(t.day) + " " + kMonthsShort[t.month - 1];
  } else {
    e.day = kWeekdays[weekdayOf(t.year, t.month, t.day)];
  }
  return e;
}

}  // namespace

int weekdayOf(int year, int month, int day) {
  const int64_t d = daysFromCivil(year, month, day);
  const int w = static_cast<int>((d + 4) % 7);  // 1970-01-01 was a Thursday
  return w < 0 ? w + 7 : w;
}

bool parseLocalTime(const std::string& in, LocalTime& out) {
  std::string s = in;
  for (char& c : s)
    if (c == 'T') c = ' ';
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
  const int n = std::sscanf(s.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &h, &mi, &sec);
  if (n < 3) return false;
  if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) return false;
  out.year = y;
  out.month = mo;
  out.day = d;
  out.hasTime = n >= 5;
  out.hour = out.hasTime ? h : 0;
  out.minute = out.hasTime ? mi : 0;
  return true;
}

DateText dateText(int64_t epoch, int32_t utcOffsetS) {
  DateText t;
  if (!plausible(epoch)) return t;
  const Civil c = civilOf(epoch, utcOffsetS);
  t.weekday = kWeekdays[weekdayOf(c.year, c.month, c.day)];
  t.date = std::to_string(c.day) + " " + kMonths[c.month - 1];
  return t;
}

std::string wakeHint(settings::SleepSetting m) {
  switch (m) {
    case settings::SleepSetting::PowerOff: return "Hold the side button for 3 s";
    case settings::SleepSetting::DeepSleep: return "Press the side button";
    case settings::SleepSetting::Touch: return "Tap the screen";
    case settings::SleepSetting::TouchAndRocker: return "Tap or press the side button";
  }
  return "";
}

Cond conditionOf(const std::string& s) {
  if (s == "sunny") return Cond::Sunny;
  if (s == "clear-night") return Cond::ClearNight;
  if (s == "partlycloudy") return Cond::PartlyCloudy;
  if (s == "cloudy") return Cond::Cloudy;
  if (s == "rainy") return Cond::Rainy;
  if (s == "pouring") return Cond::Pouring;
  if (s == "snowy" || s == "snowy-rainy" || s == "hail") return Cond::Snowy;
  if (s == "fog") return Cond::Fog;
  if (s == "windy" || s == "windy-variant") return Cond::Windy;
  if (s == "lightning" || s == "lightning-rainy") return Cond::Lightning;
  return Cond::Unknown;
}

std::string conditionText(const std::string& s) {
  static const struct {
    const char* state;
    const char* text;
  } rows[] = {{"sunny", "Sunny"},           {"clear-night", "Clear night"}, {"partlycloudy", "Partly cloudy"},
              {"cloudy", "Cloudy"},         {"rainy", "Rainy"},             {"pouring", "Pouring rain"},
              {"snowy", "Snowy"},           {"snowy-rainy", "Sleet"},       {"fog", "Fog"},
              {"hail", "Hail"},             {"windy", "Windy"},             {"windy-variant", "Windy"},
              {"lightning", "Thunderstorm"}, {"lightning-rainy", "Thunderstorm"}, {"exceptional", "Exceptional"}};
  for (const auto& r : rows)
    if (s == r.state) return r.text;
  // Unknown condition: the state with the first letter up and dashes as spaces.
  std::string o = s;
  for (char& c : o)
    if (c == '-' || c == '_') c = ' ';
  if (!o.empty() && o[0] >= 'a' && o[0] <= 'z') o[0] = static_cast<char>(o[0] - 'a' + 'A');
  return o;
}

namespace {
// Centred group: top = 16 + 2 * floor((avail - H) / 4) (spec 2); clamped at the margin.
int groupTop(int avail, int H) {
  const int d = avail - H;
  return 16 + (d > 0 ? 2 * (d / 4) : 0);
}
int blockHeight(int id) {
  switch (id) {
    case kDate: return 148;
    case kUpdated: return 84;
    case kBattery: return 128;
    case kWeather: return 84;
    case kEvent: return 108;
    case kNotifications: return 84;
    default: return 56;
  }
}
// Places `ids` in one column (x, W), vertically centred in 16..16+avail.
void column(Layout& L, const std::vector<int>& ids, int x, int W, int avail) {
  if (ids.empty()) return;
  int H = 24 * static_cast<int>(ids.size() - 1);
  for (int id : ids) H += blockHeight(id);
  int y = groupTop(avail, H);
  for (int id : ids) {
    L.blocks.push_back({id, {x, y, W, blockHeight(id)}});
    y += blockHeight(id) + 24;
  }
}
}  // namespace

Layout layoutFor(bool landscape, bool showWeather, bool showEvent, bool showNotifications) {
  Layout L;
  std::vector<int> optional;
  if (showWeather) optional.push_back(kWeather);
  if (showEvent) optional.push_back(kEvent);
  if (showNotifications) optional.push_back(kNotifications);
  if (!landscape) {
    std::vector<int> ids = {kDate, kUpdated, kBattery};
    ids.insert(ids.end(), optional.begin(), optional.end());
    column(L, ids, 16, 508, 844);  // 16 .. 860
    L.footerRule = {16, 876, 508, 1};
    L.footer = {16, 892, 508, 56};
    L.climateColumnDx = 260;
  } else {
    if (optional.empty()) {
      column(L, {kDate}, 16, 440, 448);
      column(L, {kUpdated, kBattery}, 488, 456, 448);
    } else {
      column(L, {kDate, kUpdated, kBattery}, 16, 440, 448);
      column(L, optional, 488, 456, 448);
    }
    L.divider = {472, 16, 1, 448};
    L.footerRule = {16, 476, 928, 1};
    L.footer = {16, 484, 928, 56};
    L.climateColumnDx = 224;
  }
  return L;
}

View compose(const Inputs& in) {
  View v;
  v.landscape = in.landscape;
  v.showWeather = in.weatherConfigured;
  v.showEvent = in.calendarConfigured;
  v.showNotifications = in.notificationsConfigured && in.notificationCount > 0;
  v.layout = layoutFor(in.landscape, v.showWeather, v.showEvent, v.showNotifications);
  v.date = dateText(in.wakeEpoch, in.utcOffsetS);
  v.updated = updatedText(in);
  v.battery = batteryText(in);
  v.climate = climateText(in);
  if (v.showWeather) v.weather = weatherText(in.weather);
  if (v.showEvent) v.event = eventText(in.calendar, in);
  if (v.showNotifications) {
    v.notifications.count = std::to_string(in.notificationCount);
    v.notifications.title = in.notificationTitle;
  }
  v.footerLabel = "Asleep";
  v.footerHint = wakeHint(in.sleepMode);
  return v;
}

}  // namespace sleepscreen
