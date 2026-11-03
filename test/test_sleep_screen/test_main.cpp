#include <unity.h>

#include "sleep_screen.h"

using namespace sleepscreen;

void setUp() {}
void tearDown() {}

namespace {

constexpr int64_t kTue3Nov2026_1405 = 1793714700;  // 2026-11-03T14:05:00Z (a Tuesday)

Inputs base() {
  Inputs in;
  in.wakeEpoch = kTue3Nov2026_1405;
  in.updatedEpoch = kTue3Nov2026_1405;
  in.nextUpdateEpoch = kTue3Nov2026_1405 + 85 * 60;  // 15:30
  in.batteryPct = 78;
  in.batteryMv = 3920;
  in.haveClimate = true;
  in.tempX10 = 215;
  in.humX10 = 483;
  in.sleepMode = settings::SleepSetting::TouchAndRocker;
  return in;
}

EntityState entity(const char* id, const char* state) {
  EntityState e;
  e.id = id;
  e.state = state;
  e.available = std::string(state) != "unavailable";
  return e;
}

void expectRect(int x, int y, int w, int h, const Rect& r) {
  TEST_ASSERT_EQUAL_INT(x, r.x);
  TEST_ASSERT_EQUAL_INT(y, r.y);
  TEST_ASSERT_EQUAL_INT(w, r.w);
  TEST_ASSERT_EQUAL_INT(h, r.h);
}

void expectBlock(const Layout& L, int id, int x, int y, int w, int h) {
  const Placement* p = L.find(id);
  TEST_ASSERT_NOT_NULL(p);
  expectRect(x, y, w, h, p->rect);
}

}  // namespace

// ---- layout (spec section 2) ----
void test_layout_portrait_all_blocks() {
  Layout L = layoutFor(false, true, true, true);
  expectBlock(L, kDate, 16, 60, 508, 148);
  expectBlock(L, kUpdated, 16, 232, 508, 84);
  expectBlock(L, kBattery, 16, 340, 508, 128);
  expectBlock(L, kWeather, 16, 492, 508, 84);
  expectBlock(L, kEvent, 16, 600, 508, 108);
  expectBlock(L, kNotifications, 16, 732, 508, 84);
  expectRect(16, 876, 508, 1, L.footerRule);
  expectRect(16, 892, 508, 56, L.footer);
  TEST_ASSERT_EQUAL_INT(260, L.climateColumnDx);
  TEST_ASSERT_EQUAL_INT(0, L.divider.w);
}

void test_layout_portrait_none() {
  Layout L = layoutFor(false, false, false, false);
  TEST_ASSERT_EQUAL(3, (int)L.blocks.size());
  expectBlock(L, kDate, 16, 234, 508, 148);
  expectBlock(L, kUpdated, 16, 406, 508, 84);
  expectBlock(L, kBattery, 16, 514, 508, 128);
}

void test_layout_landscape_with_optional() {
  Layout L = layoutFor(true, true, true, true);
  expectBlock(L, kDate, 16, 36, 440, 148);
  expectBlock(L, kUpdated, 16, 208, 440, 84);
  expectBlock(L, kBattery, 16, 316, 440, 128);
  expectBlock(L, kWeather, 488, 78, 456, 84);
  expectBlock(L, kEvent, 488, 186, 456, 108);
  expectBlock(L, kNotifications, 488, 318, 456, 84);
  expectRect(472, 16, 1, 448, L.divider);
  expectRect(16, 476, 928, 1, L.footerRule);
  TEST_ASSERT_EQUAL_INT(224, L.climateColumnDx);
}

void test_layout_landscape_none_splits_core() {
  Layout L = layoutFor(true, false, false, false);
  expectBlock(L, kDate, 16, 166, 440, 148);
  expectBlock(L, kUpdated, 488, 122, 456, 84);
  expectBlock(L, kBattery, 488, 230, 456, 128);
}

void test_layout_one_optional_centres() {
  Layout L = layoutFor(false, true, false, false);  // H = 148+84+128+84+72 = 516
  TEST_ASSERT_EQUAL_INT(16 + 2 * ((844 - 516) / 4), L.find(kDate)->rect.y);
  TEST_ASSERT_EQUAL(4, (int)L.blocks.size());
}

// ---- which blocks show ----
void test_blocks_only_when_configured() {
  Inputs in = base();
  View v = compose(in);
  TEST_ASSERT_FALSE(v.showWeather || v.showEvent || v.showNotifications);
  in.weatherConfigured = true;
  in.calendarConfigured = true;
  in.notificationsConfigured = true;
  in.notificationCount = 2;
  in.notificationTitle = "Disk full";
  v = compose(in);
  TEST_ASSERT_TRUE(v.showWeather && v.showEvent && v.showNotifications);
  TEST_ASSERT_EQUAL_STRING("2", v.notifications.count.c_str());
  TEST_ASSERT_EQUAL_STRING("Disk full", v.notifications.title.c_str());
  TEST_ASSERT_EQUAL(6, (int)v.layout.blocks.size());
}

void test_notifications_hidden_at_zero_or_when_off() {
  Inputs in = base();
  in.notificationsConfigured = true;
  in.notificationCount = 0;
  TEST_ASSERT_FALSE(compose(in).showNotifications);
  in.notificationsConfigured = false;
  in.notificationCount = 5;
  TEST_ASSERT_FALSE(compose(in).showNotifications);
}

// ---- texts ----
void test_date_and_updated() {
  View v = compose(base());
  TEST_ASSERT_EQUAL_STRING("Tuesday", v.date.weekday.c_str());
  TEST_ASSERT_EQUAL_STRING("3 November", v.date.date.c_str());
  TEST_ASSERT_EQUAL_STRING("updated 14:05", v.updated.line1.c_str());
  TEST_ASSERT_FALSE(v.updated.small);
  TEST_ASSERT_EQUAL_STRING("next update 15:30", v.updated.line2.c_str());
  TEST_ASSERT_EQUAL_STRING("Asleep", v.footerLabel.c_str());
}

void test_local_offset_applies() {
  Inputs in = base();
  in.utcOffsetS = 3600;  // CET
  View v = compose(in);
  TEST_ASSERT_EQUAL_STRING("updated 15:05", v.updated.line1.c_str());
  in.wakeEpoch += 10 * 3600;  // 00:05 the next day, local
  v = compose(in);
  TEST_ASSERT_EQUAL_STRING("Wednesday", v.date.weekday.c_str());
  TEST_ASSERT_EQUAL_STRING("4 November", v.date.date.c_str());
  TEST_ASSERT_EQUAL_STRING("updated 3.11. 15:05", v.updated.line1.c_str());
  TEST_ASSERT_TRUE(v.updated.small);
}

void test_updated_never_and_no_schedule() {
  Inputs in = base();
  in.updatedEpoch = 0;
  in.nextUpdateEpoch = 0;
  View v = compose(in);
  TEST_ASSERT_EQUAL_STRING("updated never", v.updated.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("no scheduled updates", v.updated.line2.c_str());
}

void test_next_update_other_day() {
  Inputs in = base();
  in.nextUpdateEpoch = kTue3Nov2026_1405 + 20 * 3600;  // 10:05 next day (UTC)
  TEST_ASSERT_EQUAL_STRING("next update 4.11. 10:05", compose(in).updated.line2.c_str());
}

void test_unknown_clock() {
  Inputs in = base();
  in.wakeEpoch = 0;
  View v = compose(in);
  TEST_ASSERT_EQUAL_STRING("", v.date.weekday.c_str());
  TEST_ASSERT_EQUAL_STRING("updated 14:05", v.updated.line1.c_str());  // no day comparison possible
}

void test_battery_texts() {
  Inputs in = base();
  View v = compose(in);
  TEST_ASSERT_EQUAL_STRING("78%", v.battery.percent.c_str());
  TEST_ASSERT_EQUAL_STRING("3.92 V", v.battery.sub.c_str());
  TEST_ASSERT_FALSE(v.battery.low);
  in.charging = true;
  TEST_ASSERT_EQUAL_STRING("charging", compose(in).battery.sub.c_str());
  in.batteryPct = 15;
  TEST_ASSERT_TRUE(compose(in).battery.low);
  in.batteryPct = 16;
  TEST_ASSERT_FALSE(compose(in).battery.low);
  in.batteryPct = 140;
  TEST_ASSERT_EQUAL_STRING("100%", compose(in).battery.percent.c_str());
}

void test_climate_and_fallback() {
  View v = compose(base());
  TEST_ASSERT_TRUE(v.climate.have);
  TEST_ASSERT_EQUAL_STRING("21.5", v.climate.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("48", v.climate.hum.c_str());
  TEST_ASSERT_EQUAL_STRING("%", v.climate.humUnit.c_str());
  Inputs in = base();
  in.tempX10 = -5;
  in.humX10 = 1000;
  v = compose(in);
  TEST_ASSERT_EQUAL_STRING("-0.5", v.climate.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("100", v.climate.hum.c_str());
  in.haveClimate = false;  // no reading: always "-", never an old value
  v = compose(in);
  TEST_ASSERT_FALSE(v.climate.have);
  TEST_ASSERT_EQUAL_STRING("-", v.climate.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("", v.climate.tempUnit.c_str());
  TEST_ASSERT_EQUAL_STRING("-", v.climate.hum.c_str());
  TEST_ASSERT_EQUAL_STRING("", v.climate.humUnit.c_str());
}

void test_wake_hint_by_mode() {
  TEST_ASSERT_EQUAL_STRING("Hold the side button for 3 s", wakeHint(settings::SleepSetting::PowerOff).c_str());
  TEST_ASSERT_EQUAL_STRING("Press the side button", wakeHint(settings::SleepSetting::DeepSleep).c_str());
  TEST_ASSERT_EQUAL_STRING("Tap the screen", wakeHint(settings::SleepSetting::Touch).c_str());
  TEST_ASSERT_EQUAL_STRING("Tap or press the side button", wakeHint(settings::SleepSetting::TouchAndRocker).c_str());
  Inputs in = base();
  in.sleepMode = settings::SleepSetting::Touch;
  TEST_ASSERT_EQUAL_STRING("Tap the screen", compose(in).footerHint.c_str());
}

// ---- weather ----
void test_weather_values() {
  Inputs in = base();
  in.weatherConfigured = true;
  EntityState w = entity("weather.home", "partlycloudy");
  w.temperature = 12.4;
  in.weather = &w;
  View v = compose(in);
  TEST_ASSERT_FALSE(v.weather.unavailable);
  TEST_ASSERT_TRUE(v.weather.cond == Cond::PartlyCloudy);
  TEST_ASSERT_EQUAL_STRING("12", v.weather.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0" "C", v.weather.unit.c_str());
  TEST_ASSERT_EQUAL_STRING("Partly cloudy", v.weather.condition.c_str());
  w.temperature = -0.3;
  w.temperature_unit = "\xC2\xB0" "F";
  v = compose(in);
  TEST_ASSERT_EQUAL_STRING("0", v.weather.temp.c_str());  // no "-0"
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0" "F", v.weather.unit.c_str());
  w.temperature.reset();
  v = compose(in);
  TEST_ASSERT_EQUAL_STRING("-", v.weather.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("", v.weather.unit.c_str());
}

void test_weather_unavailable_variants() {
  Inputs in = base();
  in.weatherConfigured = true;
  in.weather = nullptr;  // entity missing / fetch failed
  View v = compose(in);
  TEST_ASSERT_TRUE(v.showWeather);  // block kept so the layout stays
  TEST_ASSERT_TRUE(v.weather.unavailable);
  TEST_ASSERT_EQUAL_STRING("-", v.weather.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("n/a", v.weather.condition.c_str());
  EntityState u = entity("weather.home", "unavailable");
  in.weather = &u;
  TEST_ASSERT_TRUE(compose(in).weather.unavailable);
  EntityState k = entity("weather.home", "unknown");
  in.weather = &k;
  TEST_ASSERT_TRUE(compose(in).weather.unavailable);
}

void test_condition_texts() {
  TEST_ASSERT_EQUAL_STRING("Clear night", conditionText("clear-night").c_str());
  TEST_ASSERT_EQUAL_STRING("Rainy", conditionText("rainy").c_str());
  TEST_ASSERT_EQUAL_STRING("Thunderstorm", conditionText("lightning-rainy").c_str());
  TEST_ASSERT_EQUAL_STRING("Some thing", conditionText("some-thing").c_str());
  TEST_ASSERT_TRUE(conditionOf("pouring") == Cond::Pouring);
  TEST_ASSERT_TRUE(conditionOf("zzz") == Cond::Unknown);
}

// ---- calendar ----
static EntityState cal(const char* msg, const char* start, bool allDay = false) {
  EntityState e = entity("calendar.family", "on");
  if (msg) e.attrs.emplace_back("message", msg);
  if (start) e.attrs.emplace_back("start_time", start);
  if (allDay) e.attrs.emplace_back("all_day", "true");
  return e;
}

void test_calendar_timed_today_tomorrow_weekday() {
  Inputs in = base();
  in.calendarConfigured = true;
  EntityState e = cal("Dentist", "2026-11-03 16:30:00");
  in.calendar = &e;
  View v = compose(in);
  TEST_ASSERT_TRUE(v.event.kind == EventKind::Timed);
  TEST_ASSERT_EQUAL_STRING("16:30", v.event.time.c_str());
  TEST_ASSERT_EQUAL_STRING("Dentist", v.event.title.c_str());
  TEST_ASSERT_EQUAL_STRING("Today", v.event.day.c_str());
  e = cal("Dentist", "2026-11-04 09:00:00");
  TEST_ASSERT_EQUAL_STRING("Tomorrow", compose(in).event.day.c_str());
  e = cal("Dentist", "2026-11-06 09:00:00");
  TEST_ASSERT_EQUAL_STRING("Friday", compose(in).event.day.c_str());
  e = cal("Dentist", "2026-12-24 09:00:00");
  TEST_ASSERT_EQUAL_STRING("24 Dec", compose(in).event.day.c_str());
}

void test_calendar_all_day_none_unavailable() {
  Inputs in = base();
  in.calendarConfigured = true;
  EntityState a = cal("Holiday", "2026-11-03 00:00:00", true);
  in.calendar = &a;
  View v = compose(in);
  TEST_ASSERT_TRUE(v.event.kind == EventKind::AllDay);
  TEST_ASSERT_EQUAL_STRING("all day", v.event.time.c_str());
  TEST_ASSERT_EQUAL_STRING("Holiday", v.event.title.c_str());
  EntityState dateOnly = cal("Birthday", "2026-11-05");
  in.calendar = &dateOnly;
  TEST_ASSERT_TRUE(compose(in).event.kind == EventKind::AllDay);
  EntityState none = cal(nullptr, nullptr);  // calendar with nothing upcoming has no attributes
  in.calendar = &none;
  v = compose(in);
  TEST_ASSERT_TRUE(v.event.kind == EventKind::None);
  TEST_ASSERT_EQUAL_STRING("Nothing planned", v.event.time.c_str());
  in.calendar = nullptr;
  v = compose(in);
  TEST_ASSERT_TRUE(v.showEvent);
  TEST_ASSERT_TRUE(v.event.kind == EventKind::Unavailable);
  TEST_ASSERT_EQUAL_STRING("Calendar n/a", v.event.time.c_str());
  EntityState un = entity("calendar.family", "unavailable");
  in.calendar = &un;
  TEST_ASSERT_TRUE(compose(in).event.kind == EventKind::Unavailable);
}

void test_calendar_weekday_without_clock() {
  Inputs in = base();
  in.wakeEpoch = 0;
  in.calendarConfigured = true;
  EntityState e = cal("Dentist", "2026-11-06 09:00:00");
  in.calendar = &e;
  TEST_ASSERT_EQUAL_STRING("Friday", compose(in).event.day.c_str());
}

void test_parse_local_time() {
  LocalTime t;
  TEST_ASSERT_TRUE(parseLocalTime("2026-11-03 16:30:00", t));
  TEST_ASSERT_TRUE(t.hasTime);
  TEST_ASSERT_EQUAL_INT(16, t.hour);
  TEST_ASSERT_TRUE(parseLocalTime("2026-11-03T16:30:00", t));
  TEST_ASSERT_TRUE(t.hasTime);
  TEST_ASSERT_TRUE(parseLocalTime("2026-11-03", t));
  TEST_ASSERT_FALSE(t.hasTime);
  TEST_ASSERT_FALSE(parseLocalTime("nonsense", t));
  TEST_ASSERT_FALSE(parseLocalTime("2026-13-03 10:00:00", t));
  TEST_ASSERT_EQUAL_INT(2, weekdayOf(2026, 11, 3));  // Tuesday
  TEST_ASSERT_EQUAL_INT(4, weekdayOf(1970, 1, 1));   // Thursday
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_layout_portrait_all_blocks);
  RUN_TEST(test_layout_portrait_none);
  RUN_TEST(test_layout_landscape_with_optional);
  RUN_TEST(test_layout_landscape_none_splits_core);
  RUN_TEST(test_layout_one_optional_centres);
  RUN_TEST(test_blocks_only_when_configured);
  RUN_TEST(test_notifications_hidden_at_zero_or_when_off);
  RUN_TEST(test_date_and_updated);
  RUN_TEST(test_local_offset_applies);
  RUN_TEST(test_updated_never_and_no_schedule);
  RUN_TEST(test_next_update_other_day);
  RUN_TEST(test_unknown_clock);
  RUN_TEST(test_battery_texts);
  RUN_TEST(test_climate_and_fallback);
  RUN_TEST(test_wake_hint_by_mode);
  RUN_TEST(test_weather_values);
  RUN_TEST(test_weather_unavailable_variants);
  RUN_TEST(test_condition_texts);
  RUN_TEST(test_calendar_timed_today_tomorrow_weekday);
  RUN_TEST(test_calendar_all_day_none_unavailable);
  RUN_TEST(test_calendar_weekday_without_clock);
  RUN_TEST(test_parse_local_time);
  return UNITY_END();
}
