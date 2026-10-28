#include <unity.h>

#include <ArduinoJson.h>
#include "config_model.h"  // first include of the config lib: keeps the PlatformIO LDF order that resolves its ArduinoJson dependency
#include "ha_worker.h"
#include "mailbox.h"
#include "session_driver.h"
#include "sleep_logic.h"
#include "wall_time.h"

using namespace app_logic;
using power::WakeReason;

void setUp() {}
void tearDown() {}

static WakeInput in(ResetKind r, WakePin p = WakePin::None, bool flag = false, bool marker = false) {
  WakeInput w;
  w.reset = r; w.pin = p; w.rtc_alarm_flag = flag; w.slept_marker = marker;
  return w;
}

void test_wake_power_on() {
  TEST_ASSERT_EQUAL(int(WakeReason::ColdBoot), int(classifyWake(in(ResetKind::PowerOn))));
  TEST_ASSERT_EQUAL(int(WakeReason::Button), int(classifyWake(in(ResetKind::PowerOn, WakePin::None, false, true))));
  TEST_ASSERT_EQUAL(int(WakeReason::RtcAlarm), int(classifyWake(in(ResetKind::PowerOn, WakePin::None, true, true))));
  // a stale flag without our marker is still the alarm: only the RTC can set it
  TEST_ASSERT_EQUAL(int(WakeReason::RtcAlarm), int(classifyWake(in(ResetKind::PowerOn, WakePin::None, true, false))));
}

void test_wake_deep_sleep() {
  TEST_ASSERT_EQUAL(int(WakeReason::Touch), int(classifyWake(in(ResetKind::DeepSleep, WakePin::Touch))));
  TEST_ASSERT_EQUAL(int(WakeReason::Button), int(classifyWake(in(ResetKind::DeepSleep, WakePin::Rocker))));
  TEST_ASSERT_EQUAL(int(WakeReason::RtcAlarm), int(classifyWake(in(ResetKind::DeepSleep, WakePin::Timer))));
  TEST_ASSERT_EQUAL(int(WakeReason::RtcAlarm), int(classifyWake(in(ResetKind::DeepSleep, WakePin::None, true))));
  TEST_ASSERT_EQUAL(int(WakeReason::Unknown), int(classifyWake(in(ResetKind::DeepSleep))));
  // a touch that arrives together with the alarm: the pin wins (the user is waiting)
  TEST_ASSERT_EQUAL(int(WakeReason::Touch), int(classifyWake(in(ResetKind::DeepSleep, WakePin::Touch, true))));
}

void test_wake_restarts_are_not_wakes() {
  for (ResetKind k : {ResetKind::Software, ResetKind::Crash, ResetKind::External, ResetKind::Other})
    TEST_ASSERT_EQUAL(int(WakeReason::ColdBoot), int(classifyWake(in(k, WakePin::None, true, true))));
}

void test_session_kind_from_wake() {
  TEST_ASSERT_EQUAL(int(power::Session::ScheduledRefresh), int(power::decide_session(WakeReason::RtcAlarm, 4000)));
  TEST_ASSERT_EQUAL(int(power::Session::Interactive), int(power::decide_session(WakeReason::Touch, 4000)));
  TEST_ASSERT_EQUAL(int(power::Session::LowBattery), int(power::decide_session(WakeReason::RtcAlarm, 3200)));
}

void test_indicator() {
  power::SleepPlan b;  // Button mode
  power::SleepPlan t;
  t.mode = power::SleepMode::DeepSleep; t.touch_wake = true;
  TEST_ASSERT_EQUAL(int(Indicator::Button), int(indicatorFor(b, power::Session::Interactive)));
  TEST_ASSERT_EQUAL(int(Indicator::Touch), int(indicatorFor(t, power::Session::Interactive)));
  TEST_ASSERT_EQUAL(int(Indicator::Touch), int(indicatorFor(t, power::Session::ScheduledRefresh)));
  // low battery: touch wake is off, so the button variant in any mode
  TEST_ASSERT_EQUAL(int(Indicator::Button), int(indicatorFor(t, power::Session::LowBattery)));
  TEST_ASSERT_EQUAL(int(Indicator::Touch), int(indicatorFromByte(uint8_t(Indicator::Touch))));
  TEST_ASSERT_EQUAL(int(Indicator::None), int(indicatorFromByte(77)));
}

// 2026-10-28 10:00:00 UTC
static const int64_t kNow = 1793181600;

void test_alarm_fields() {
  TEST_ASSERT_EQUAL(0, Civil{}.second);
  const Civil c = epochToCivil(kNow, 0);
  TEST_ASSERT_EQUAL(10, c.hour);
  auto f = alarmFields(kNow + 3600, kNow);
  TEST_ASSERT_TRUE(f.has_value());
  TEST_ASSERT_EQUAL(0, f->minute);
  TEST_ASSERT_EQUAL(11, f->hour);
  TEST_ASSERT_EQUAL(28, f->day);
  // rounded up to the next whole minute
  f = alarmFields(kNow + 3601, kNow);
  TEST_ASSERT_EQUAL(1, f->minute);
  // across midnight and month end
  f = alarmFields(kNow + 22 * 3600 + 30 * 60, kNow);  // 2026-10-29 08:30
  TEST_ASSERT_EQUAL(29, f->day);
  TEST_ASSERT_EQUAL(8, f->hour);
  TEST_ASSERT_EQUAL(30, f->minute);
  f = alarmFields(kNow + 4 * 24 * 3600, kNow);  // 2026-11-01
  TEST_ASSERT_EQUAL(1, f->day);
}

void test_alarm_fields_rejects() {
  TEST_ASSERT_FALSE(alarmFields(std::nullopt, kNow).has_value());
  TEST_ASSERT_FALSE(alarmFields(kNow + 3600, 0).has_value());           // clock unset
  TEST_ASSERT_FALSE(alarmFields(kNow - 10, kNow).has_value());           // in the past
  TEST_ASSERT_FALSE(alarmFields(kNow, kNow).has_value());                // now is not the future
  TEST_ASSERT_FALSE(alarmFields(kNow + 28LL * 24 * 3600, kNow).has_value());  // beyond the day-of-month range
  TEST_ASSERT_TRUE(alarmFields(kNow + 26LL * 24 * 3600, kNow).has_value());
}

void test_path_button_mode() {
  power::SleepPlan p;
  p.rtc_alarm_at = kNow + 3600;
  const SleepPath s = planSleepPath(p, power::Session::Interactive, kNow);
  TEST_ASSERT_EQUAL(int(SleepMethod::PowerOff), int(s.first));
  TEST_ASSERT_TRUE(s.arm_rtc_alarm);
  TEST_ASSERT_TRUE(s.wake_rocker);
  TEST_ASSERT_FALSE(s.wake_touch);
  TEST_ASSERT_TRUE(s.wake_timer_s.has_value());
  TEST_ASSERT_EQUAL(3600, int(*s.wake_timer_s));
  TEST_ASSERT_EQUAL(int(Indicator::Button), int(s.indicator));
  TEST_ASSERT_FALSE(s.low_battery);
}

void test_path_touch_mode() {
  power::SleepPlan p;
  p.mode = power::SleepMode::DeepSleep; p.touch_wake = true;
  p.rtc_alarm_at = kNow + 600;
  const SleepPath s = planSleepPath(p, power::Session::Interactive, kNow);
  TEST_ASSERT_EQUAL(int(SleepMethod::DeepSleep), int(s.first));
  TEST_ASSERT_TRUE(s.wake_touch);
  TEST_ASSERT_TRUE(s.wake_rocker);
  TEST_ASSERT_EQUAL(int(Indicator::Touch), int(s.indicator));
  TEST_ASSERT_EQUAL(600, int(*s.wake_timer_s));
}

void test_path_no_alarm_cases() {
  power::SleepPlan p;  // refresh off
  SleepPath s = planSleepPath(p, power::Session::Interactive, kNow);
  TEST_ASSERT_FALSE(s.arm_rtc_alarm);
  TEST_ASSERT_FALSE(s.wake_timer_s.has_value());
  p.rtc_alarm_at = kNow + 3600;
  s = planSleepPath(p, power::Session::Interactive, 0);  // clock unset: no alarm possible
  TEST_ASSERT_FALSE(s.arm_rtc_alarm);
  TEST_ASSERT_FALSE(s.wake_timer_s.has_value());
}

void test_path_low_battery() {
  power::SleepPlan p;
  p.mode = power::SleepMode::DeepSleep; p.touch_wake = true;
  p.rtc_alarm_at = kNow + 3600;  // even if a plan carried one
  const SleepPath s = planSleepPath(p, power::Session::LowBattery, kNow);
  TEST_ASSERT_TRUE(s.low_battery);
  TEST_ASSERT_EQUAL(int(SleepMethod::PowerOff), int(s.first));
  TEST_ASSERT_FALSE(s.arm_rtc_alarm);
  TEST_ASSERT_FALSE(s.wake_touch);
  TEST_ASSERT_TRUE(s.wake_rocker);
  TEST_ASSERT_FALSE(s.wake_timer_s.has_value());
}

void test_path_from_real_plan() {
  settings::Settings st;  // defaults: button mode, 60 min
  const power::SleepPlan p = power::plan_sleep(st, power::Session::Interactive, kNow, 3600);
  const SleepPath s = planSleepPath(p, power::Session::Interactive, kNow);
  TEST_ASSERT_TRUE(s.arm_rtc_alarm);
  // next full local hour: 11:00 UTC = 12:00 local
  TEST_ASSERT_EQUAL(11, s.alarm->hour);
  TEST_ASSERT_EQUAL(0, s.alarm->minute);
}

void test_guard_button_wake() {
  WakeInputGuard g;
  g.arm(WakeReason::Button, 1000);
  TEST_ASSERT_TRUE(g.blocksKey());
  TEST_ASSERT_FALSE(g.blocksTouch());
  g.observe(1010, true, false);   // still held
  g.observe(1500, true, false);
  TEST_ASSERT_TRUE(g.blocksKey());
  g.observe(1600, false, false);  // released
  g.observe(1700, false, false);
  TEST_ASSERT_TRUE(g.blocksKey());
  g.observe(1760, false, false);  // up for >= 150 ms
  TEST_ASSERT_FALSE(g.blocksKey());
}

void test_guard_bounce_restarts_settle() {
  WakeInputGuard g;
  g.arm(WakeReason::Button, 0);
  g.observe(100, false, false);
  g.observe(200, true, false);  // pressed again
  g.observe(300, false, false);
  g.observe(400, false, false);
  TEST_ASSERT_TRUE(g.blocksKey());
  g.observe(450, false, false);
  TEST_ASSERT_FALSE(g.blocksKey());
}

void test_guard_touch_wake_and_timeout() {
  WakeInputGuard g;
  g.arm(WakeReason::Touch, 0);
  TEST_ASSERT_TRUE(g.blocksTouch());
  TEST_ASSERT_FALSE(g.blocksKey());
  g.observe(100, false, true);
  g.observe(2900, false, true);  // finger stays down
  TEST_ASSERT_TRUE(g.blocksTouch());
  g.observe(3000, false, true);  // hard cap
  TEST_ASSERT_FALSE(g.blocksTouch());
}

void test_guard_other_wakes_block_nothing() {
  WakeInputGuard g;
  for (WakeReason r : {WakeReason::ColdBoot, WakeReason::RtcAlarm, WakeReason::Unknown}) {
    g.arm(r, 0);
    TEST_ASSERT_FALSE(g.blocksKey());
    TEST_ASSERT_FALSE(g.blocksTouch());
  }
}

// ---- tester additions (review of task 024) ----

void test_wake_other_pin_is_unknown_even_with_flag() {
  TEST_ASSERT_EQUAL(int(WakeReason::Unknown), int(classifyWake(in(ResetKind::DeepSleep, WakePin::Other))));
  TEST_ASSERT_EQUAL(int(WakeReason::Unknown), int(classifyWake(in(ResetKind::DeepSleep, WakePin::Other, true, true))));
  // a deep-sleep wake never depends on the NVS marker
  TEST_ASSERT_EQUAL(int(WakeReason::Unknown), int(classifyWake(in(ResetKind::DeepSleep, WakePin::None, false, true))));
}

void test_wake_every_reset_pin_combination_is_defined() {
  // Exhaustive: no combination may return something other than a valid reason, and non-DeepSleep resets
  // never depend on the pin.
  for (int r = 0; r <= int(ResetKind::Other); ++r)
    for (int p = 0; p <= int(WakePin::Other); ++p)
      for (int f = 0; f < 2; ++f)
        for (int m = 0; m < 2; ++m) {
          const WakeReason w = classifyWake(in(ResetKind(r), WakePin(p), f, m));
          TEST_ASSERT_TRUE(int(w) >= int(WakeReason::ColdBoot) && int(w) <= int(WakeReason::Unknown));
          if (ResetKind(r) != ResetKind::DeepSleep && ResetKind(r) != ResetKind::PowerOn)
            TEST_ASSERT_EQUAL(int(WakeReason::ColdBoot), int(w));
        }
}

void test_alarm_fields_month_year_leap_crossings() {
  Civil c;
  c.year = 2026; c.month = 10; c.day = 31; c.hour = 23; c.minute = 30;
  int64_t now = civilToEpoch(c);
  auto f = alarmFields(now + 3600, now);  // 00:30 on Nov 1
  TEST_ASSERT_TRUE(f.has_value());
  TEST_ASSERT_EQUAL(1, f->day); TEST_ASSERT_EQUAL(0, f->hour); TEST_ASSERT_EQUAL(30, f->minute);

  c.month = 12; c.day = 31; c.hour = 23; c.minute = 50;  // year end
  now = civilToEpoch(c);
  f = alarmFields(now + 20 * 60, now);
  TEST_ASSERT_EQUAL(1, f->day); TEST_ASSERT_EQUAL(0, f->hour); TEST_ASSERT_EQUAL(10, f->minute);

  c.year = 2028; c.month = 2; c.day = 28; c.hour = 12; c.minute = 0;  // leap year: Feb 29 exists
  now = civilToEpoch(c);
  f = alarmFields(now + 24 * 3600, now);
  TEST_ASSERT_EQUAL(29, f->day);
  c.year = 2027;  // not a leap year: Feb 28 -> Mar 1
  now = civilToEpoch(c);
  f = alarmFields(now + 24 * 3600, now);
  TEST_ASSERT_EQUAL(1, f->day);
}

void test_alarm_fields_is_utc_not_local() {
  // The planner works in local time but the RTC is written in UTC: the fields must be UTC.
  // 2026-10-28 10:00 UTC alarm at +3 h = 13:00 UTC even though local (CET, +1) would be 14:00.
  const auto f = alarmFields(kNow + 3 * 3600, kNow);
  TEST_ASSERT_EQUAL(13, f->hour);
}

void test_alarm_fields_rounding_and_limit_boundaries() {
  const int64_t lim = kMaxAlarmAheadS;
  TEST_ASSERT_TRUE(alarmFields(kNow + lim, kNow).has_value());            // exactly 27 d: allowed
  TEST_ASSERT_FALSE(alarmFields(kNow + lim + 1, kNow).has_value());       // rounds up past the limit
  // seconds are rounded up, never down (an alarm must not fire before the planned instant)
  auto f = alarmFields(kNow + 3600 + 59, kNow);
  TEST_ASSERT_EQUAL(1, f->minute);
  f = alarmFields(kNow + 3600 + 60, kNow);
  TEST_ASSERT_EQUAL(1, f->minute);
  // sub-minute request from a mid-minute "now": next whole minute
  f = alarmFields(kNow + 30 + 10, kNow + 30);
  TEST_ASSERT_TRUE(f.has_value());
  TEST_ASSERT_EQUAL(1, f->minute);
  // rounding crosses the hour
  f = alarmFields(kNow + 3599 + 3600, kNow);  // 10:59:59 + 1 h -> 12:00
  TEST_ASSERT_EQUAL(12, f->hour); TEST_ASSERT_EQUAL(0, f->minute);
  // implausible clock just under the threshold
  TEST_ASSERT_FALSE(alarmFields(kMinPlausibleEpoch + 3600, kMinPlausibleEpoch - 1).has_value());
}

void test_path_alarm_beyond_limit_has_no_timer_either() {
  power::SleepPlan p;
  p.mode = power::SleepMode::DeepSleep; p.touch_wake = true;
  p.rtc_alarm_at = kNow + 40LL * 24 * 3600;
  const SleepPath s = planSleepPath(p, power::Session::Interactive, kNow);
  TEST_ASSERT_FALSE(s.arm_rtc_alarm);
  TEST_ASSERT_FALSE(s.alarm.has_value());
  TEST_ASSERT_FALSE(s.wake_timer_s.has_value());
  TEST_ASSERT_TRUE(s.wake_touch);  // still wakes on touch
}

void test_path_scheduled_session_keeps_plan_wake_sources() {
  power::SleepPlan p;
  p.rtc_alarm_at = kNow + 3600;
  const SleepPath s = planSleepPath(p, power::Session::ScheduledRefresh, kNow);
  TEST_ASSERT_EQUAL(int(SleepMethod::PowerOff), int(s.first));
  TEST_ASSERT_TRUE(s.arm_rtc_alarm);
  TEST_ASSERT_FALSE(s.low_battery);
  TEST_ASSERT_EQUAL(int(Indicator::Button), int(s.indicator));
}

void test_path_timer_matches_rtc_alarm_instant() {
  // The timer stand-in is the plan's instant minus now, not the minute-rounded one; it must never be < 1 s.
  power::SleepPlan p;
  p.rtc_alarm_at = kNow + 1;
  SleepPath s = planSleepPath(p, power::Session::Interactive, kNow);
  TEST_ASSERT_TRUE(s.wake_timer_s.has_value());
  TEST_ASSERT_TRUE(*s.wake_timer_s >= 1);
  TEST_ASSERT_EQUAL(1, s.alarm->minute);
}

void test_guard_boundaries() {
  WakeInputGuard g;
  g.arm(WakeReason::Button, 0);
  g.observe(10, false, false);
  g.observe(10 + WakeInputGuard::kSettleMs - 1, false, false);
  TEST_ASSERT_TRUE(g.blocksKey());  // one ms short of the settle time
  g.observe(10 + WakeInputGuard::kSettleMs, false, false);
  TEST_ASSERT_FALSE(g.blocksKey());
  g.arm(WakeReason::Button, 5000);  // re-arming after a first release blocks again
  TEST_ASSERT_TRUE(g.blocksKey());
  // held key: released only by the cap, exactly at kMaxMs
  g.observe(5000 + WakeInputGuard::kMaxMs - 1, true, false);
  TEST_ASSERT_TRUE(g.blocksKey());
  g.observe(5000 + WakeInputGuard::kMaxMs, true, false);
  TEST_ASSERT_FALSE(g.blocksKey());
}

void test_guard_key_wake_ignores_touch_and_touch_wake_ignores_key() {
  WakeInputGuard g;
  g.arm(WakeReason::Button, 0);
  g.observe(10, false, true);  // a finger on the screen must not extend the key guard
  g.observe(200, false, true);
  TEST_ASSERT_FALSE(g.blocksKey());
  g.arm(WakeReason::Touch, 0);
  g.observe(10, true, false);  // the rocker held must not extend the touch guard
  g.observe(200, true, false);
  TEST_ASSERT_FALSE(g.blocksTouch());
}

void test_guard_not_armed_blocks_nothing() {
  WakeInputGuard g;
  g.observe(100, true, true);
  TEST_ASSERT_FALSE(g.blocksKey());
  TEST_ASSERT_FALSE(g.blocksTouch());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wake_power_on);
  RUN_TEST(test_wake_deep_sleep);
  RUN_TEST(test_wake_restarts_are_not_wakes);
  RUN_TEST(test_session_kind_from_wake);
  RUN_TEST(test_indicator);
  RUN_TEST(test_alarm_fields);
  RUN_TEST(test_alarm_fields_rejects);
  RUN_TEST(test_path_button_mode);
  RUN_TEST(test_path_touch_mode);
  RUN_TEST(test_path_no_alarm_cases);
  RUN_TEST(test_path_low_battery);
  RUN_TEST(test_path_from_real_plan);
  RUN_TEST(test_guard_button_wake);
  RUN_TEST(test_guard_bounce_restarts_settle);
  RUN_TEST(test_guard_touch_wake_and_timeout);
  RUN_TEST(test_guard_other_wakes_block_nothing);
  RUN_TEST(test_wake_other_pin_is_unknown_even_with_flag);
  RUN_TEST(test_wake_every_reset_pin_combination_is_defined);
  RUN_TEST(test_alarm_fields_month_year_leap_crossings);
  RUN_TEST(test_alarm_fields_is_utc_not_local);
  RUN_TEST(test_alarm_fields_rounding_and_limit_boundaries);
  RUN_TEST(test_path_alarm_beyond_limit_has_no_timer_either);
  RUN_TEST(test_path_scheduled_session_keeps_plan_wake_sources);
  RUN_TEST(test_path_timer_matches_rtc_alarm_instant);
  RUN_TEST(test_guard_boundaries);
  RUN_TEST(test_guard_key_wake_ignores_touch_and_touch_wake_ignores_key);
  RUN_TEST(test_guard_not_armed_blocks_nothing);
  return UNITY_END();
}
