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
Session decide_session(WakeReason reason, int32_t battery_mv);

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

// Piecewise-linear Li-ion curve, monotonic, clamped to 0..100.
int battery_percent(int32_t mv);

}  // namespace power
