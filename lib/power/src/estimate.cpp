#include "estimate.h"


namespace power {
namespace est {

namespace {
double restMa(settings::SleepSetting m) {
    switch (m) {
        case settings::SleepSetting::PowerOff: return kRestPowerOffMa;
        case settings::SleepSetting::DeepSleep: return kRestDeepMa;
        case settings::SleepSetting::Touch: return kRestTouchMa;
        case settings::SleepSetting::TouchAndRocker: return kRestTouchRockerMa;
    }
    return kRestDeepMa;
}

double sleepingMa(settings::SleepSetting mode, int32_t interval_min, int32_t idle_s) {
    const double cold = mode == settings::SleepSetting::PowerOff ? kColdStartExtraS : 0.0;
    double mah = restMa(mode) * 24.0;  // mAh per day
    mah += kUseWakesPerDay * (kUseBaseAwakeS + idle_s + cold) * kActiveMa / 3600.0;
    if (interval_min > 0) mah += (24.0 * 60.0 / interval_min) * (kScheduledAwakeS + cold) * kActiveMa / 3600.0;
    return mah / 24.0;
}
}  // namespace

double averageMa(const settings::Settings& s, const settings::Capabilities& c) {
    if (s.keep_awake == settings::KeepAwake::Always) return kAlwaysAwakeMa;
    return sleepingMa(s.sleep_mode, settings::effectiveRefreshMin(s, c), s.idle_timeout_s);
}

int batteryUseLevel(const settings::Settings& s, const settings::Capabilities& c) {
    if (s.keep_awake == settings::KeepAwake::Always) return 10;
    int base = 5;
    switch (s.sleep_mode) {
        case settings::SleepSetting::PowerOff: base = 1; break;
        case settings::SleepSetting::DeepSleep: base = 5; break;
        case settings::SleepSetting::Touch:
        case settings::SleepSetting::TouchAndRocker: base = 6; break;
    }
    const int32_t iv = settings::effectiveRefreshMin(s, c);
    const int updates = iv <= 0 ? 0 : (iv <= 60 ? 2 : 1);
    const int idle = s.idle_timeout_s >= 30 ? 1 : 0;
    return base + updates + idle;
}

}  // namespace est
}  // namespace power
