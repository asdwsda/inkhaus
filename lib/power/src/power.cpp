#include "power.h"

#include <algorithm>

namespace power {

Session decide_session(WakeReason reason, int32_t battery_mv) {
    if (battery_mv < kLowBatteryMv) return Session::LowBattery;
    if (reason == WakeReason::RtcAlarm) return Session::ScheduledRefresh;
    return Session::Interactive;
}

void IdleTimer::start(int64_t now) {
    last_input_ = now;
    busy_ = false;
}
void IdleTimer::on_input(int64_t now) { last_input_ = now; }

bool IdleTimer::should_sleep(int64_t now) const {
    const int64_t idle = now - last_input_;
    const int64_t cap = std::max<int64_t>(kIdleHardCapS, timeout_s_);
    if (idle >= cap) return true;
    return !busy_ && idle >= timeout_s_;
}

static int64_t floor_div(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

SleepPlan plan_sleep(const settings::Settings& s, Session session, int64_t now,
                     int32_t utc_offset_s) {
    SleepPlan p;
    if (s.wake_mode == settings::WakeMode::Touch || s.wake_mode == settings::WakeMode::TouchAndButton) {
        p.mode = SleepMode::DeepSleep;
        p.touch_wake = true;
        p.button_wake = s.wake_mode == settings::WakeMode::TouchAndButton;
    } else {
        p.mode = SleepMode::PowerOff;
        p.touch_wake = false;
        p.button_wake = true;
    }
    if (s.refresh_interval_min > 0 && session != Session::LowBattery) {
        const int64_t iv = static_cast<int64_t>(s.refresh_interval_min) * 60;
        const int64_t local = now + utc_offset_s;
        int64_t next = (floor_div(local, iv) + 1) * iv;
        if (next - local < kMinAlarmLeadS) next += iv;
        p.rtc_alarm_at = next - utc_offset_s;
    }
    return p;
}

int battery_percent(int32_t mv) {
    struct Pt { int32_t mv; int pct; };
    static const Pt curve[] = {{3300, 0},  {3500, 5},  {3600, 15}, {3700, 35},
                               {3800, 55}, {3900, 70}, {4000, 82}, {4100, 92},
                               {4200, 100}};
    constexpr int n = sizeof(curve) / sizeof(curve[0]);
    if (mv <= curve[0].mv) return 0;
    if (mv >= curve[n - 1].mv) return 100;
    for (int i = 1; i < n; ++i) {
        if (mv <= curve[i].mv) {
            const int32_t dmv = curve[i].mv - curve[i - 1].mv;
            const int dp = curve[i].pct - curve[i - 1].pct;
            return curve[i - 1].pct + (dp * (mv - curve[i - 1].mv) + dmv / 2) / dmv;
        }
    }
    return 100;
}

}  // namespace power
