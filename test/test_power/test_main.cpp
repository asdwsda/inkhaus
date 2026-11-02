#include <cstdint>
#include <string>
#include <unity.h>

#include "estimate.h"
#include "power.h"

using namespace power;
using settings::Settings;
using settings::SleepSetting;

namespace {
constexpr int64_t kMidnight = 1767225600;  // 2026-01-01 00:00:00 UTC
Settings mk(SleepSetting w, int32_t refresh) {
    Settings s;
    s.sleep_mode = w;
    s.refresh_interval_min = refresh;
    return s;
}
int64_t alarm(int32_t interval_min, int64_t now, int32_t off = 0) {
    auto p = plan_sleep(mk(SleepSetting::PowerOff, interval_min), Session::Interactive, now, off);
    TEST_ASSERT_TRUE(p.rtc_alarm_at.has_value());
    return *p.rtc_alarm_at;
}
}  // namespace

void setUp() {}
void tearDown() {}

void test_session_table() {
    const WakeReason rs[] = {WakeReason::ColdBoot, WakeReason::Button, WakeReason::RtcAlarm,
                             WakeReason::Touch, WakeReason::Unknown};
    for (auto r : rs) {
        Session normal = r == WakeReason::RtcAlarm ? Session::ScheduledRefresh : Session::Interactive;
        TEST_ASSERT_EQUAL(normal, decide_session(r, 3900));
        TEST_ASSERT_EQUAL(normal, decide_session(r, kLowBatteryMv));  // threshold itself is OK
        TEST_ASSERT_EQUAL(Session::LowBattery, decide_session(r, kLowBatteryMv - 1));
    }
}

void test_idle_sleeps_after_timeout() {
    IdleTimer t(20);
    t.start(1000);
    TEST_ASSERT_FALSE(t.should_sleep(1019));
    TEST_ASSERT_TRUE(t.should_sleep(1020));
    t.on_input(1015);
    TEST_ASSERT_FALSE(t.should_sleep(1020));
    TEST_ASSERT_FALSE(t.should_sleep(1034));
    TEST_ASSERT_TRUE(t.should_sleep(1035));
}

void test_idle_busy_delays_sleep() {
    IdleTimer t(20);
    t.start(0);
    t.set_busy(true);
    TEST_ASSERT_FALSE(t.should_sleep(20));
    TEST_ASSERT_FALSE(t.should_sleep(100));
    t.set_busy(false);
    TEST_ASSERT_TRUE(t.should_sleep(100));
}

void test_idle_hard_cap_when_busy() {
    IdleTimer t(20);
    t.start(0);
    t.set_busy(true);
    TEST_ASSERT_FALSE(t.should_sleep(kIdleHardCapS - 1));
    TEST_ASSERT_TRUE(t.should_sleep(kIdleHardCapS));
    // input resets the cap
    t.on_input(100);
    TEST_ASSERT_FALSE(t.should_sleep(100 + kIdleHardCapS - 1));
    TEST_ASSERT_TRUE(t.should_sleep(100 + kIdleHardCapS));
}

void test_idle_cap_not_below_timeout() {
    IdleTimer t(200);
    t.start(0);
    TEST_ASSERT_FALSE(t.should_sleep(199));
    TEST_ASSERT_TRUE(t.should_sleep(200));
}

void test_plan_button_mode() {
    auto p = plan_sleep(mk(SleepSetting::PowerOff, 60), Session::Interactive, kMidnight + 100, 0);
    TEST_ASSERT_EQUAL(power::SleepMode::PowerOff, p.mode);
    TEST_ASSERT_TRUE(p.button_wake);
    TEST_ASSERT_FALSE(p.touch_wake);
    TEST_ASSERT_TRUE(p.rtc_alarm_at.has_value());
}

void test_plan_touch_mode() {
    auto p = plan_sleep(mk(SleepSetting::TouchAndRocker, 60), Session::Interactive, kMidnight + 100, 0);
    TEST_ASSERT_EQUAL(power::SleepMode::DeepSleep, p.mode);
    TEST_ASSERT_TRUE(p.button_wake);
    TEST_ASSERT_TRUE(p.touch_wake);
    TEST_ASSERT_TRUE(p.rtc_alarm_at.has_value());
}

void test_plan_touch_only_and_both() {
    auto t = plan_sleep(mk(SleepSetting::Touch, 60), Session::Interactive, kMidnight + 100, 0);
    TEST_ASSERT_EQUAL(power::SleepMode::DeepSleep, t.mode);
    TEST_ASSERT_TRUE(t.touch_wake);
    TEST_ASSERT_FALSE(t.button_wake);
    auto b = plan_sleep(mk(SleepSetting::TouchAndRocker, 60), Session::Interactive, kMidnight + 100, 0);
    TEST_ASSERT_EQUAL(power::SleepMode::DeepSleep, b.mode);
    TEST_ASSERT_TRUE(b.touch_wake);
    TEST_ASSERT_TRUE(b.button_wake);
}

void test_plan_refresh_off_and_low_battery() {
    for (auto w : {SleepSetting::PowerOff, SleepSetting::DeepSleep, SleepSetting::Touch}) {
        auto off = plan_sleep(mk(w, 0), Session::Interactive, kMidnight, 0);
        TEST_ASSERT_FALSE(off.rtc_alarm_at.has_value());
        auto low = plan_sleep(mk(w, 60), Session::LowBattery, kMidnight, 0);
        TEST_ASSERT_FALSE(low.rtc_alarm_at.has_value());
    }
    // wake sources are unaffected by missing alarm
    auto p = plan_sleep(mk(SleepSetting::Touch, 0), Session::LowBattery, kMidnight, 0);
    TEST_ASSERT_TRUE(p.touch_wake);
}

void test_alarm_boundaries() {
    const int64_t now = kMidnight + 10 * 3600 + 7 * 60 + 30;  // 10:07:30
    TEST_ASSERT_EQUAL_INT64(kMidnight + 10 * 3600 + 15 * 60, alarm(15, now));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 10 * 3600 + 30 * 60, alarm(30, now));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 11 * 3600, alarm(60, now));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 12 * 3600, alarm(120, now));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 12 * 3600, alarm(240, now));
    const int64_t now2 = kMidnight + 13 * 3600;  // exactly on a boundary -> next one
    TEST_ASSERT_EQUAL_INT64(kMidnight + 14 * 3600, alarm(60, now2));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 16 * 3600, alarm(240, now2));
}

void test_alarm_min_lead() {
    // 30 s before the hour: skip to the following hour
    TEST_ASSERT_EQUAL_INT64(kMidnight + 12 * 3600, alarm(60, kMidnight + 10 * 3600 + 3570));
    // exactly 60 s before: allowed
    TEST_ASSERT_EQUAL_INT64(kMidnight + 11 * 3600, alarm(60, kMidnight + 10 * 3600 + 3540));
    // 59 s before: skipped
    TEST_ASSERT_EQUAL_INT64(kMidnight + 12 * 3600, alarm(60, kMidnight + 10 * 3600 + 3541));
    // 15 min near boundary
    TEST_ASSERT_EQUAL_INT64(kMidnight + 30 * 60, alarm(15, kMidnight + 15 * 60 + 1));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 30 * 60, alarm(15, kMidnight + 14 * 60 + 30));
}

void test_alarm_local_offset() {
    // Local UTC+5:30 (19800 s), 120 min interval: boundaries at even local hours.
    const int32_t off = 19800;
    const int64_t now = kMidnight + 3600;  // 01:00 UTC = 06:30 local
    // next even local hour = 08:00 local = 02:30 UTC
    TEST_ASSERT_EQUAL_INT64(kMidnight + 2 * 3600 + 1800, alarm(120, now, off));
    // Negative offset UTC-8, 60 min: now 10:20 UTC = 02:20 local -> 03:00 local = 11:00 UTC
    TEST_ASSERT_EQUAL_INT64(kMidnight + 11 * 3600, alarm(60, kMidnight + 10 * 3600 + 1200, -28800));
    // 60 min with a 30-min-offset zone: boundary is at xx:30 UTC
    TEST_ASSERT_EQUAL_INT64(kMidnight + 1800, alarm(60, kMidnight + 600, off));
}

void test_battery_percent() {
    TEST_ASSERT_EQUAL(0, battery_percent(0));
    TEST_ASSERT_EQUAL(0, battery_percent(3300));
    TEST_ASSERT_EQUAL(100, battery_percent(4200));
    TEST_ASSERT_EQUAL(100, battery_percent(5000));
    int prev = 0;
    for (int32_t mv = 2500; mv <= 4500; ++mv) {
        int p = battery_percent(mv);
        TEST_ASSERT_TRUE(p >= 0 && p <= 100);
        TEST_ASSERT_TRUE(p >= prev);
        prev = p;
    }
}

void test_alarm_pre_1970_local() {
    // now = 1970-01-01 01:00 UTC, offset -8h -> local = -7h (before 1970), 60 min
    // local -25200 is on a boundary -> next local -21600 -> UTC = -21600 + 28800 = 7200
    TEST_ASSERT_EQUAL_INT64(7200, alarm(60, 3600, -28800));
    // local = -7h + 20 min -> next boundary still -21600
    TEST_ASSERT_EQUAL_INT64(7200, alarm(60, 3600 + 1200, -28800));
    // negative now, UTC: -100 s -> next 60-min boundary is 0, 100 s lead OK
    TEST_ASSERT_EQUAL_INT64(0, alarm(60, -100));
    // negative now, 30 s before boundary -> skipped to +3600
    TEST_ASSERT_EQUAL_INT64(3600, alarm(60, -30));
    // deeply negative, not whole multiple
    TEST_ASSERT_EQUAL_INT64(-3600, alarm(60, -3601 - 100));
}
void test_alarm_odd_offsets() {
    // +5:45 = 20700 s, 60 min: boundaries at xx:15 UTC
    TEST_ASSERT_EQUAL_INT64(kMidnight + 900, alarm(60, kMidnight + 10, 20700));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 900 + 3600, alarm(60, kMidnight + 900, 20700));
    // +5:30, 15 min: boundaries aligned to local quarter hours == UTC quarters
    TEST_ASSERT_EQUAL_INT64(kMidnight + 900, alarm(15, kMidnight + 1, 19800));
    // -3:30 (Newfoundland), 30 min
    TEST_ASSERT_EQUAL_INT64(kMidnight + 1800, alarm(30, kMidnight + 1, -12600));
    // UTC+14 and UTC-12 extremes, 240 min: result is on a local 4 h boundary, > now+60
    for (int32_t off : {50400, -43200}) {
        for (int64_t now = kMidnight; now < kMidnight + 2 * 86400; now += 997) {
            int64_t a = alarm(240, now, off);
            TEST_ASSERT_TRUE(a - now >= 60);
            TEST_ASSERT_TRUE(a - now <= 240 * 60 + 60);
            TEST_ASSERT_EQUAL_INT64(0, ((a + off) % (240 * 60) + 240 * 60) % (240 * 60));
        }
    }
}
void test_alarm_exact_boundary_and_lead_across_midnight() {
    // UTC, now exactly on local midnight -> next boundary one interval later
    TEST_ASSERT_EQUAL_INT64(kMidnight + 3600, alarm(60, kMidnight));
    TEST_ASSERT_EQUAL_INT64(kMidnight + 14400, alarm(240, kMidnight));
    // 23:59:30 local, 60 min: midnight is 30 s away -> skipped to 01:00 next day
    TEST_ASSERT_EQUAL_INT64(kMidnight + 86400 + 3600, alarm(60, kMidnight + 86370));
    // 23:59:00 exactly 60 s before midnight -> midnight allowed
    TEST_ASSERT_EQUAL_INT64(kMidnight + 86400, alarm(60, kMidnight + 86340));
    // same with local offset -5h: local midnight at 05:00 UTC
    TEST_ASSERT_EQUAL_INT64(kMidnight + 86400 + 5 * 3600,
                            alarm(60, kMidnight + 86400 + 5 * 3600 - 60, -18000));
    // 59 s before local midnight -> skipped
    TEST_ASSERT_EQUAL_INT64(kMidnight + 86400 + 6 * 3600,
                            alarm(60, kMidnight + 86400 + 5 * 3600 - 59, -18000));
}
void test_alarm_property_all_intervals() {
    for (int32_t iv : {15, 30, 60, 120, 240}) {
        for (int32_t off : {0, 19800, 20700, -28800, -12600}) {
            for (int64_t now = -200000; now < 200000; now += 313) {
                int64_t a = alarm(iv, now, off);
                TEST_ASSERT_TRUE(a - now >= 60);
                TEST_ASSERT_TRUE(a - now <= iv * 60 + 60);
                int64_t m = (a + off) % (iv * 60);
                TEST_ASSERT_EQUAL_INT64(0, m);
            }
        }
    }
}
void test_idle_time_backwards() {
    IdleTimer t(20);
    t.start(1000);
    // clock stepped back: must not sleep immediately, and not overflow
    TEST_ASSERT_FALSE(t.should_sleep(500));
    // documented behaviour: stays awake until clock catches up (>= last_input + timeout)
    TEST_ASSERT_TRUE(t.should_sleep(1020));
}
void test_idle_busy_continuous_input_cap() {
    IdleTimer t(20);
    t.start(0);
    t.set_busy(true);
    t.on_input(100);
    // hard cap is from last input: 100+120
    TEST_ASSERT_FALSE(t.should_sleep(219));
    TEST_ASSERT_TRUE(t.should_sleep(220));
}
void test_battery_extremes() {
    TEST_ASSERT_EQUAL(0, battery_percent(INT32_MIN));
    TEST_ASSERT_EQUAL(100, battery_percent(INT32_MAX));
    TEST_ASSERT_EQUAL(0, battery_percent(-1));
}

// ---- stay awake on USB (task 081) ----

void test_stay_awake_needs_setting_and_usb() {
    Settings s;
    s.sleep_mode = SleepSetting::PowerOff;  // On USB is offered here
    TEST_ASSERT_FALSE(stay_awake(s, true));  // default off
    s.keep_awake = settings::KeepAwake::OnUsb;  // default mode is Power off: offered
    TEST_ASSERT_TRUE(stay_awake(s, true));
    TEST_ASSERT_FALSE(stay_awake(s, false));
    s.keep_awake = settings::KeepAwake::Always;  // never sleeps, USB or not
    TEST_ASSERT_TRUE(stay_awake(s, false));
    TEST_ASSERT_TRUE(stay_awake(s, true));
    // The USB probe is reliable (task 085): On USB works in the deep-sleep modes too
    s.keep_awake = settings::KeepAwake::OnUsb;
    s.sleep_mode = SleepSetting::DeepSleep;
    TEST_ASSERT_TRUE(stay_awake(s, true));
    TEST_ASSERT_FALSE(stay_awake(s, false));
}

void test_plan_per_sleep_mode() {
    auto po = plan_sleep(mk(SleepSetting::PowerOff, 180), Session::Interactive, kMidnight + 100, 0);
    TEST_ASSERT_EQUAL(power::SleepMode::PowerOff, po.mode);
    TEST_ASSERT_TRUE(po.button_wake);
    TEST_ASSERT_FALSE(po.touch_wake);
    auto ds = plan_sleep(mk(SleepSetting::DeepSleep, 180), Session::Interactive, kMidnight + 100, 0);
    TEST_ASSERT_EQUAL(power::SleepMode::DeepSleep, ds.mode);
    TEST_ASSERT_TRUE(ds.button_wake);
    TEST_ASSERT_FALSE(ds.touch_wake);
    // updates Off: no alarm in any mode
    for (auto m : settings::kSleepModeValues)
        TEST_ASSERT_FALSE(plan_sleep(mk(m, 0), Session::Interactive, kMidnight + 100, 0).rtc_alarm_at.has_value());
}

void test_plan_forced_off_updates_without_rtc_power_on() {
    // Capabilities are a compile-time constant (rtc_power_on = true): the rule itself is tested in test_settings.
    settings::Capabilities c;
    c.rtc_power_on = false;
    Settings s = mk(SleepSetting::PowerOff, 180);
    TEST_ASSERT_EQUAL_INT32(0, settings::effectiveRefreshMin(s, c));
    s.sleep_mode = SleepSetting::DeepSleep;
    TEST_ASSERT_EQUAL_INT32(180, settings::effectiveRefreshMin(s, c));
}

// Drives the idle timer like main does: feed when the gate says so, sleep when the timer says so.
static int64_t firstSleepAt(const Settings& s, bool (*usbAt)(int64_t), int64_t until) {
    IdleTimer idle(s.idle_timeout_s);
    StayAwakeGate gate;
    idle.start(0);
    for (int64_t t = 1; t <= until; ++t) {
        if (gate.feed_due(s, usbAt(t), t)) idle.on_input(t);
        if (idle.should_sleep(t)) return t;
    }
    return -1;
}

void test_no_idle_sleep_while_on_usb_with_setting() {
    Settings s;
    s.sleep_mode = SleepSetting::PowerOff;  // On USB is offered here
    s.idle_timeout_s = 20;
    s.keep_awake = settings::KeepAwake::OnUsb;
    TEST_ASSERT_EQUAL_INT64(-1, firstSleepAt(s, [](int64_t) { return true; }, 20 * 10));  // > 2x timeout
    s.keep_awake = settings::KeepAwake::Off;  // setting off: as before
    TEST_ASSERT_EQUAL_INT64(20, firstSleepAt(s, [](int64_t) { return true; }, 200));
    s.keep_awake = settings::KeepAwake::OnUsb;   // no USB: as before
    TEST_ASSERT_EQUAL_INT64(20, firstSleepAt(s, [](int64_t) { return false; }, 200));
}

void test_unplug_restarts_idle_timer() {
    Settings s;
    s.sleep_mode = SleepSetting::PowerOff;  // On USB is offered here
    s.idle_timeout_s = 20;
    s.keep_awake = settings::KeepAwake::OnUsb;
    // on USB for 100 s, unplugged after that
    const int64_t at = firstSleepAt(s, [](int64_t t) { return t <= 100; }, 400);
    TEST_ASSERT_TRUE(at >= 100 + 20 && at <= 100 + 20 + StayAwakeGate::kKeepAliveS + 1);
}

void test_no_refresh_alarm_while_staying_awake() {
    // Staying awake never reaches plan_sleep: no sleep is due, so no alarm is armed.
    Settings s;
    s.sleep_mode = SleepSetting::PowerOff;  // On USB is offered here
    s.keep_awake = settings::KeepAwake::OnUsb;
    s.refresh_interval_min = 60;
    TEST_ASSERT_EQUAL_INT64(-1, firstSleepAt(s, [](int64_t) { return true; }, 3600));
    // A manual sleep on USB still plans normally (alarm kept).
    SleepPlan p = plan_sleep(s, Session::Interactive, kMidnight + 100, 0);
    TEST_ASSERT_TRUE(p.rtc_alarm_at.has_value());
}

void test_gate_feed_rate_limited() {
    Settings s;
    s.sleep_mode = SleepSetting::PowerOff;  // On USB is offered here
    s.keep_awake = settings::KeepAwake::OnUsb;
    StayAwakeGate g;
    TEST_ASSERT_TRUE(g.feed_due(s, true, 100));
    TEST_ASSERT_FALSE(g.feed_due(s, true, 100));
    TEST_ASSERT_TRUE(g.feed_due(s, true, 101));
    TEST_ASSERT_FALSE(g.feed_due(s, false, 200));
}

void test_low_battery_helper() {
    TEST_ASSERT_TRUE(below_low_battery(kLowBatteryMv - 1));
    TEST_ASSERT_FALSE(below_low_battery(kLowBatteryMv));
}

void test_usb_at_sleep_flag() {
    TEST_ASSERT_FALSE(power::usbAtSleep());
    power::set_usb_at_sleep(true);
    TEST_ASSERT_TRUE(power::usbAtSleep());
    power::set_usb_at_sleep(false);
    TEST_ASSERT_FALSE(power::usbAtSleep());
}

void test_usb_present_probe_first() {
    power::set_usb_probe(-1);
    power::set_usb_at_sleep(true);
    TEST_ASSERT_TRUE(power::usbPresent());  // no probe reading: fallback
    power::set_usb_probe(0);
    TEST_ASSERT_FALSE(power::usbPresent());  // unplug seen although the flag is stale
    power::set_usb_at_sleep(false);
    power::set_usb_probe(1);
    TEST_ASSERT_TRUE(power::usbPresent());
    power::set_usb_probe(-1);
    TEST_ASSERT_FALSE(power::usbPresent());
}

// ---- battery estimate (task 085) ----

void test_battery_level() {
    using namespace power::est;
    Settings best = mk(SleepSetting::PowerOff, 0);
    best.idle_timeout_s = 10;
    TEST_ASSERT_EQUAL_INT(1, batteryUseLevel(best));
    Settings al = mk(SleepSetting::PowerOff, 0);
    al.keep_awake = settings::KeepAwake::Always;
    TEST_ASSERT_EQUAL_INT(10, batteryUseLevel(al));
    // Power off: Updates Off vs 30 min differ by at least one level
    TEST_ASSERT_TRUE(batteryUseLevel(mk(SleepSetting::PowerOff, 30)) >= batteryUseLevel(mk(SleepSetting::PowerOff, 0)) + 1);
    Settings worst = mk(SleepSetting::TouchAndRocker, 30);
    worst.idle_timeout_s = 60;
    TEST_ASSERT_EQUAL_INT(9, batteryUseLevel(worst));
    {
        Settings a = mk(SleepSetting::PowerOff, 0), b = a;
        a.idle_timeout_s = 10;
        b.idle_timeout_s = 60;
        TEST_ASSERT_TRUE(batteryUseLevel(b) >= batteryUseLevel(a) + 1);
        Settings h = mk(SleepSetting::PowerOff, 60);
        h.idle_timeout_s = 10;
        Settings o = h;
        o.refresh_interval_min = 0;
        TEST_ASSERT_TRUE(batteryUseLevel(h) >= batteryUseLevel(o) + 1);
    }
    // defaults
    TEST_ASSERT_EQUAL_INT(9, batteryUseLevel(Settings{}));
    // monotonic in sleep mode, interval (shorter never lower), idle timeout, keep awake
    for (int32_t iv : settings::kRefreshIntervalMinValues) {
        int prev = 0;
        for (auto m : settings::kSleepModeValues) {
            const int l = batteryUseLevel(mk(m, iv));
            TEST_ASSERT_TRUE(l >= prev);
            prev = l;
        }
    }
    for (auto m : settings::kSleepModeValues) {
        int prev = 0;
        for (int32_t iv : {0, 720, 360, 180, 60, 30}) {
            const int l = batteryUseLevel(mk(m, iv));
            TEST_ASSERT_TRUE(l >= prev);
            prev = l;
        }
        prev = 0;
        for (int32_t idle : settings::kIdleTimeoutSValues) {
            Settings x = mk(m, 60);
            x.idle_timeout_s = idle;
            const int l = batteryUseLevel(x);
            TEST_ASSERT_TRUE(l >= prev);
            prev = l;
        }
        Settings x = mk(m, 60);
        const int off = batteryUseLevel(x);
        x.keep_awake = settings::KeepAwake::OnUsb;
        TEST_ASSERT_TRUE(batteryUseLevel(x) >= off);
        x.keep_awake = settings::KeepAwake::Always;
        TEST_ASSERT_EQUAL_INT(10, batteryUseLevel(x));
    }
    // every non-Always configuration stays below 10 and within 1..9
    TEST_ASSERT_TRUE(batteryUseLevel(mk(SleepSetting::TouchAndRocker, 30)) <= 9);
    // forced-Off updates: same as Off
    settings::Capabilities noRtc;
    noRtc.rtc_power_on = false;
    TEST_ASSERT_EQUAL_INT(batteryUseLevel(mk(SleepSetting::PowerOff, 0)), batteryUseLevel(mk(SleepSetting::PowerOff, 30), noRtc));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_session_table);
    RUN_TEST(test_idle_sleeps_after_timeout);
    RUN_TEST(test_idle_busy_delays_sleep);
    RUN_TEST(test_idle_hard_cap_when_busy);
    RUN_TEST(test_idle_cap_not_below_timeout);
    RUN_TEST(test_plan_button_mode);
    RUN_TEST(test_plan_touch_mode);
    RUN_TEST(test_plan_touch_only_and_both);
    RUN_TEST(test_plan_refresh_off_and_low_battery);
    RUN_TEST(test_alarm_boundaries);
    RUN_TEST(test_alarm_min_lead);
    RUN_TEST(test_alarm_local_offset);
    RUN_TEST(test_battery_percent);
    RUN_TEST(test_alarm_pre_1970_local);
    RUN_TEST(test_alarm_odd_offsets);
    RUN_TEST(test_alarm_exact_boundary_and_lead_across_midnight);
    RUN_TEST(test_alarm_property_all_intervals);
    RUN_TEST(test_idle_time_backwards);
    RUN_TEST(test_idle_busy_continuous_input_cap);
    RUN_TEST(test_battery_extremes);
    RUN_TEST(test_stay_awake_needs_setting_and_usb);
    RUN_TEST(test_plan_per_sleep_mode);
    RUN_TEST(test_plan_forced_off_updates_without_rtc_power_on);
    RUN_TEST(test_no_idle_sleep_while_on_usb_with_setting);
    RUN_TEST(test_unplug_restarts_idle_timer);
    RUN_TEST(test_no_refresh_alarm_while_staying_awake);
    RUN_TEST(test_gate_feed_rate_limited);
    RUN_TEST(test_low_battery_helper);
    RUN_TEST(test_usb_at_sleep_flag);
    RUN_TEST(test_usb_present_probe_first);
    RUN_TEST(test_battery_level);
    return UNITY_END();
}
