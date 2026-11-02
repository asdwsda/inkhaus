#include "estimate.h"

#include <algorithm>
#include <cmath>

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
    const double best = sleepingMa(settings::SleepSetting::PowerOff, 0, settings::kIdleTimeoutSValues[0]);
    const double t = std::log(averageMa(s, c) / best) / std::log(kAlwaysAwakeMa / best);
    return std::clamp(1 + static_cast<int>(std::lround(9.0 * t)), 1, 9);
}

}  // namespace est
}  // namespace power
