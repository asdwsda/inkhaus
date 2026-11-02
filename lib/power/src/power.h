#pragma once
// Power policy: pure decision logic, no Arduino includes, no hardware access.
// All times are epoch seconds (int64_t, UTC) unless stated otherwise.
#include <cstdint>
#include <optional>

#include "settings.h"

namespace power {

enum class WakeReason { ColdBoot, Button, RtcAlarm, Touch, Unknown };
enum class Session { Interactive, ScheduledRefresh, LowBattery };

constexpr int32_t kLowBatteryMv = 3300;
constexpr int32_t kIdleHardCapS = 120;
constexpr int64_t kMinAlarmLeadS = 60;

// Battery below kLowBatteryMv -> LowBattery; RtcAlarm -> ScheduledRefresh; else Interactive.
Session decide_session(WakeReason reason, int32_t battery_mv, int32_t limit_mv = kLowBatteryMv);

// Idle timer. Feed it the current time on every call (fake time friendly).
// Sleep is due when:
//  - not busy and now - last_input >= idle_timeout_s, or
//  - (even when busy) now - last_input >= max(kIdleHardCapS, idle_timeout_s).
// The hard cap is measured from the last input, so continuous use never hits it.
class IdleTimer {
public:
    explicit IdleTimer(int32_t idle_timeout_s = 20) : timeout_s_(idle_timeout_s) {}
    void start(int64_t now);  // session start counts as an input
    void on_input(int64_t now);
    void set_busy(bool busy) { busy_ = busy; }
    bool busy() const { return busy_; }
    bool should_sleep(int64_t now) const;

private:
    int32_t timeout_s_;
    int64_t last_input_ = 0;
    bool busy_ = false;
};

enum class SleepMode { PowerOff, DeepSleep };

struct SleepPlan {
    SleepMode mode = SleepMode::PowerOff;
    std::optional<int64_t> rtc_alarm_at;  // epoch s (UTC)
    bool touch_wake = false;
    bool button_wake = true;
};

// utc_offset_s: local time = UTC + utc_offset_s. The alarm is aligned to interval
// boundaries of local time since local midnight (60 min -> next full local hour),
// strictly after now and at least kMinAlarmLeadS in the future. The result is UTC.
// No alarm if refresh_interval_min <= 0 or session == LowBattery.
SleepPlan plan_sleep(const settings::Settings& s, Session session, int64_t now,
                     int32_t utc_offset_s);

// ---- USB power / stay awake (task 081) ----

// Task 084: true if the last power-off attempt found the board still running (GPIO2 low did not cut the power):
// USB supplies it. Set by sleepwake::enter(), restored after a deep sleep; false when unknown (never tested).
inline bool& usb_at_sleep_ref() {
    static bool v = false;
    return v;
}
inline bool usbAtSleep() { return usb_at_sleep_ref(); }
inline void set_usb_at_sleep(bool v) { usb_at_sleep_ref() = v; }

// Idle sleep is held off only when the setting is on AND the device is on USB power.
inline bool stay_awake(const settings::Settings& s, bool usb_powered) { return s.stay_awake_usb && usb_powered; }

// While staying awake the idle timer is fed (as if the user touched the device) at most once per
// kKeepAliveS, so the timer never runs out. After an unplug the feeding stops and the normal idle timeout
// starts from the last feed (<= kKeepAliveS ago). Pure: the caller does idle.on_input(now) when true.
class StayAwakeGate {
public:
    static constexpr int64_t kKeepAliveS = 1;
    bool feed_due(const settings::Settings& s, bool usb_powered, int64_t now_s);

private:
    int64_t last_feed_ = -1000000;
};

// The one place for the low-battery test. limit_mv is the effective (learned or default) limit, task 079.
inline bool below_low_battery(int32_t mv, int32_t limit_mv = kLowBatteryMv) { return mv < limit_mv; }

// No VBUS or charger-status signal is readable on the M5Paper (M5Unified isCharging/getVBUSVoltage are
// unsupported for it), so USB power is inferred from the battery voltage (UNVERIFIED heuristic). A fresh
// cell reads HIGHER (4.39-4.41 V) than USB (~4.32-4.35 V), so no absolute level proves USB. Rule:
//  - every wake starts as "not USB"; `charging` true is trusted and means USB at once;
//  - judged over a sliding window of kWindowS seconds of smoothed samples (drop = oldest - newest):
//    drop <= kFlatMv (flat or rising) and mV >= kMinUsbMv -> USB; drop >= kFallMv -> battery;
//    in between -> keep the previous state, but at most kHoldCapS since the last "flat" verdict;
//  - nothing is judged until the window spans >= kMinSpanS. Feed it every few seconds.
class UsbDetector {
public:
    static constexpr int64_t kWindowS = 120;
    static constexpr int64_t kMinSpanS = 100;
    static constexpr int32_t kFlatMv = 8;
    static constexpr int32_t kFallMv = 20;
    static constexpr int32_t kMinUsbMv = 3900;
    static constexpr int64_t kHoldCapS = 3600;
    bool update(int64_t now_s, int32_t battery_mv, bool charging);
    bool usb() const { return usb_; }
    int32_t smoothed_mv() const { return smooth_; }

private:
    static constexpr int kMax = 64;
    bool usb_ = false;
    bool have_ = false;
    int32_t smooth16_ = 0;  // exponentially smoothed mV x16
    int32_t smooth_ = 0;    // the same, rounded to mV
    int64_t confirm_t_ = 0;
    int64_t t_[kMax] = {};
    int32_t v_[kMax] = {};
    int n_ = 0;
};

// Piecewise-linear Li-ion curve, monotonic, clamped to 0..100.
int battery_percent(int32_t mv);

}  // namespace power
