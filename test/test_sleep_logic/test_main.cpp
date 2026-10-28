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
  return UNITY_END();
}
