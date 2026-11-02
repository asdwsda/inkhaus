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

void test_splash_and_kept_image() {
  using R = WakeReason;
  // power-off wake by rocker or by the BM8563 alarm: no splash, image kept
  for (R w : {R::Button, R::RtcAlarm}) {
    TEST_ASSERT_FALSE(splashWanted(ResetKind::PowerOn, w));
    TEST_ASSERT_TRUE(wokeFromSleep(ResetKind::PowerOn, w));
  }
  TEST_ASSERT_TRUE(splashWanted(ResetKind::PowerOn, R::ColdBoot));
  TEST_ASSERT_FALSE(wokeFromSleep(ResetKind::PowerOn, R::ColdBoot));
  TEST_ASSERT_TRUE(splashWanted(ResetKind::External, R::ColdBoot));
  TEST_ASSERT_TRUE(wokeFromSleep(ResetKind::DeepSleep, R::Touch));
  TEST_ASSERT_FALSE(splashWanted(ResetKind::DeepSleep, R::Touch));
  // alarm flag after a power-off: scheduled, not interactive
  const R w = classifyWake(in(ResetKind::PowerOn, WakePin::None, true, true));
  TEST_ASSERT_EQUAL(int(power::Session::ScheduledRefresh), int(power::decide_session(w, 4000)));
}

void test_restore_tab_decision() {
  // task 091: every sleep wake (timer wake incl. the one with the periodic full, touch, rocker, power-off alarm) restores the tab
  for (WakeReason w : {WakeReason::RtcAlarm, WakeReason::Touch, WakeReason::Button}) {
    TEST_ASSERT_TRUE(restoreTabWanted(ResetKind::DeepSleep, w));
    TEST_ASSERT_TRUE(restoreTabWanted(ResetKind::PowerOn, w));
  }
  TEST_ASSERT_TRUE(restoreTabWanted(ResetKind::DeepSleep, classifyWake(in(ResetKind::DeepSleep, WakePin::Timer))));
  // a restart inside a session (software, crash, brownout = Other) keeps the tab
  for (ResetKind r : {ResetKind::Software, ResetKind::Crash, ResetKind::Other})
    TEST_ASSERT_TRUE(restoreTabWanted(r, classifyWake(in(r))));
  // reset button and real cold boot start on the first tab
  TEST_ASSERT_FALSE(restoreTabWanted(ResetKind::External, WakeReason::ColdBoot));
  TEST_ASSERT_FALSE(restoreTabWanted(ResetKind::PowerOn, WakeReason::ColdBoot));
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
  TEST_ASSERT_EQUAL(int(Indicator::PowerOff), int(indicatorFromByte(1)));
  TEST_ASSERT_EQUAL(int(Indicator::Deep), int(indicatorFromByte(2)));
  TEST_ASSERT_EQUAL(int(Indicator::Tap), int(indicatorFromByte(3)));
  TEST_ASSERT_EQUAL(int(Indicator::None), int(indicatorFromByte(0)));
  TEST_ASSERT_EQUAL(int(Indicator::None), int(indicatorFromByte(77)));
  // the pill follows the sleep mode, also for the low-battery sleep (mode as set)
  power::SleepPlan b;  // Power off
  power::SleepPlan d;
  d.mode = power::SleepMode::DeepSleep;
  power::SleepPlan t;
  t.mode = power::SleepMode::DeepSleep; t.touch_wake = true;
  for (auto sess : {power::Session::Interactive, power::Session::ScheduledRefresh, power::Session::LowBattery}) {
    TEST_ASSERT_EQUAL(int(Indicator::PowerOff), int(planSleepPath(b, sess, 1700000000).indicator));
    TEST_ASSERT_EQUAL(int(Indicator::Deep), int(planSleepPath(d, sess, 1700000000).indicator));
    TEST_ASSERT_EQUAL(int(Indicator::Tap), int(planSleepPath(t, sess, 1700000000).indicator));
  }
}

void test_pill_next_minute() {
  const int64_t now = 1767225600 + 10 * 3600;  // 10:00 UTC
  TEST_ASSERT_EQUAL(-1, pillNextMinute(std::nullopt, now, 0));
  TEST_ASSERT_EQUAL(10 * 60 + 30, pillNextMinute(now + 1800, now, 0));
  TEST_ASSERT_EQUAL(12 * 60 + 30, pillNextMinute(now + 1800, now, 7200));  // local = UTC + 2 h
  TEST_ASSERT_EQUAL(22 * 60, pillNextMinute(now + 12 * 3600, now, 0));
  TEST_ASSERT_EQUAL(-1, pillNextMinute(now + 12 * 3600 + 1, now, 0));  // beyond 12 h: no date on the pill
  TEST_ASSERT_EQUAL(-1, pillNextMinute(now - 60, now, 0));
  TEST_ASSERT_EQUAL(23 * 60 + 30, localMinuteOfDay(1767225600 - 1800, 0));
  TEST_ASSERT_EQUAL(30, localMinuteOfDay(1767225600 - 1800, 3600));
  // the path carries the next update only when an alarm is armed
  power::SleepPlan p;
  TEST_ASSERT_FALSE(planSleepPath(p, power::Session::Interactive, now).next_update_at.has_value());
  p.rtc_alarm_at = now + 3600;
  TEST_ASSERT_EQUAL_INT64(now + 3600, *planSleepPath(p, power::Session::Interactive, now).next_update_at);
  TEST_ASSERT_FALSE(planSleepPath(p, power::Session::LowBattery, now).next_update_at.has_value());
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
  TEST_ASSERT_EQUAL(int(Indicator::PowerOff), int(s.indicator));
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
  TEST_ASSERT_EQUAL(int(Indicator::Tap), int(s.indicator));
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
  TEST_ASSERT_EQUAL(int(Indicator::PowerOff), int(s.indicator));
}

void test_exec_resolution() {
  SleepExec e = resolveSleepExec(SleepMethod::PowerOff, false);
  TEST_ASSERT_FALSE(e.gpio2_low);
  TEST_ASSERT_TRUE(e.epd_standby);
  e = resolveSleepExec(SleepMethod::PowerOff, true);
  TEST_ASSERT_TRUE(e.gpio2_low);
  e = resolveSleepExec(SleepMethod::DeepSleep, true);
  TEST_ASSERT_FALSE(e.gpio2_low);
  TEST_ASSERT_TRUE(e.epd_standby);
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

void test_local_idle_plan() {
  TEST_ASSERT_TRUE(app_logic::localIdleSleepWanted(false));
  TEST_ASSERT_FALSE(app_logic::localIdleSleepWanted(true));
  settings::Settings s;
  s.refresh_interval_min = 60;
  s.sleep_mode = settings::SleepSetting::TouchAndRocker;
  auto p = app_logic::localIdlePlan(s, 1700000000, 3600);
  TEST_ASSERT_FALSE(p.rtc_alarm_at.has_value());
  TEST_ASSERT_TRUE(p.touch_wake);
  TEST_ASSERT_TRUE(p.button_wake);
  s.sleep_mode = settings::SleepSetting::PowerOff;
  p = app_logic::localIdlePlan(s, 1700000000, 0);
  TEST_ASSERT_FALSE(p.rtc_alarm_at.has_value());
  TEST_ASSERT_FALSE(p.touch_wake);
  }

void test_ext1_empty_mask_is_rocker() {
  TEST_ASSERT_TRUE(pinForExt1(0) == WakePin::Rocker);
  TEST_ASSERT_TRUE(pinForExt1(1ULL << 38) == WakePin::Rocker);
  TEST_ASSERT_EQUAL(int(WakeReason::Button), int(classifyWake(in(ResetKind::DeepSleep, pinForExt1(0)))));
}

void test_sleep_plan_uses_current_settings_and_per_mode_sources() {
  settings::Settings boot;  // boot-time copy: Button
  settings::Settings now = boot;
  now.sleep_mode = settings::SleepSetting::Touch;  // changed in Settings during the session
  auto sess = power::Session::Interactive;
  auto stale = power::plan_sleep(boot, sess, 1700000000, 0);
  auto cur = planForSleep(now, sess, 1700000000, 0, false);
  TEST_ASSERT_FALSE(stale.touch_wake);
  TEST_ASSERT_TRUE(cur.touch_wake);
  TEST_ASSERT_FALSE(cur.button_wake);
  auto sp = planSleepPath(cur, sess, 1700000000);
  TEST_ASSERT_TRUE(sp.wake_touch);
  TEST_ASSERT_FALSE(sp.wake_rocker);
  now.sleep_mode = settings::SleepSetting::PowerOff;
  cur = planForSleep(now, sess, 1700000000, 0, false);
  sp = planSleepPath(cur, sess, 1700000000);
  TEST_ASSERT_TRUE(sp.wake_rocker);
  TEST_ASSERT_FALSE(sp.wake_touch);
  now.sleep_mode = settings::SleepSetting::TouchAndRocker;
  cur = planForSleep(now, sess, 1700000000, 0, true);
  TEST_ASSERT_FALSE(cur.rtc_alarm_at.has_value());
  sp = planSleepPath(cur, sess, 1700000000);
  TEST_ASSERT_TRUE(sp.wake_touch);
  TEST_ASSERT_TRUE(sp.wake_rocker);
}

void test_guard_not_armed_blocks_nothing() {
  WakeInputGuard g;
  g.observe(100, true, true);
  TEST_ASSERT_FALSE(g.blocksKey());
  TEST_ASSERT_FALSE(g.blocksTouch());
}

void test_refresh_decision() {
  // scheduled wake: nothing during the session; the GC16 full at sleep is unconditional
  TEST_ASSERT_TRUE(deferRefreshes(true));
  TEST_ASSERT_FALSE(deferRefreshes(false));
  // interactive wake: first frame full only with debt and Updates Off
  TEST_ASSERT_TRUE(firstFrameFull(false, true, false));
  TEST_ASSERT_FALSE(firstFrameFull(false, true, true));   // a scheduled wake will clear it
  TEST_ASSERT_FALSE(firstFrameFull(false, false, false));
  TEST_ASSERT_FALSE(firstFrameFull(true, true, false));   // scheduled wake always ends with the full
}

void test_idle_full_due() {
  const uint32_t t = kIdleFullMs;
  TEST_ASSERT_TRUE(idleFullDue(true, t, false, false));
  TEST_ASSERT_FALSE(idleFullDue(true, t - 1, false, false));  // input restarted the timer
  TEST_ASSERT_FALSE(idleFullDue(false, t * 3, false, false)); // no debt: no repeat after the full
  TEST_ASSERT_FALSE(idleFullDue(true, t, true, false));       // popup / Settings / OTA
  TEST_ASSERT_FALSE(idleFullDue(true, t, false, true));       // input pending
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ext1_empty_mask_is_rocker);
  RUN_TEST(test_sleep_plan_uses_current_settings_and_per_mode_sources);
  RUN_TEST(test_wake_power_on);
  RUN_TEST(test_splash_and_kept_image);
  RUN_TEST(test_wake_deep_sleep);
  RUN_TEST(test_restore_tab_decision);
  RUN_TEST(test_wake_restarts_are_not_wakes);
  RUN_TEST(test_session_kind_from_wake);
  RUN_TEST(test_indicator);
  RUN_TEST(test_pill_next_minute);
  RUN_TEST(test_alarm_fields);
  RUN_TEST(test_alarm_fields_rejects);
  RUN_TEST(test_path_button_mode);
  RUN_TEST(test_exec_resolution);
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
  RUN_TEST(test_local_idle_plan);
  RUN_TEST(test_guard_boundaries);
  RUN_TEST(test_guard_key_wake_ignores_touch_and_touch_wake_ignores_key);
  RUN_TEST(test_guard_not_armed_blocks_nothing);
  RUN_TEST(test_refresh_decision);
  RUN_TEST(test_idle_full_due);
  return UNITY_END();
}
