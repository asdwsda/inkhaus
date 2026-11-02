#pragma once
// "Battery use" level 1..10 for the Sleep & wake settings page (task 085, docs/ui/settings.md 8.2). The model
// (average current from the settings) stays internal; only the level is shown. ALL CONSTANTS ARE ESTIMATES until
// a current meter measures them; only the ranking matters.
#include "settings.h"

namespace power {
namespace est {

constexpr double kRestDeepMa = 13.0;        // deep sleep, everything powered (calibrated on the 2026-10-30..11-02 wake log)
constexpr double kRestTouchMa = 13.7;       // touch wake: same hardware plus spurious wakes (assumption)
constexpr double kRestTouchRockerMa = 13.7;
constexpr double kRestPowerOffMa = 0.10;    // BM8563 + leakage + self-discharge (assumption)
constexpr double kActiveMa = 120.0;         // awake with Wi-Fi (assumption)
constexpr double kScheduledAwakeS = 6.7;    // scheduled wake, measured
constexpr double kColdStartExtraS = 1.5;    // power-off wake starts cold
constexpr int kUseWakesPerDay = 10;         // assumed daily use
constexpr double kUseBaseAwakeS = 20.0;     // per use wake: this + the idle timeout (20 s idle -> 40 s)
constexpr double kAlwaysAwakeMa = 80.0;     // Keep awake Always

// Model: average current in mA (forced-Off updates respected).
double averageMa(const settings::Settings& s, const settings::Capabilities& c = settings::kCapabilities);
// Level 1..10: Keep awake Always = 10; sleeping setups: log scale over 1..9 from the best (Power off, Updates Off,
// idle 10 s = 1) to the worst (Touch & rocker, 30 min, idle 60 s = 9).
int batteryUseLevel(const settings::Settings& s, const settings::Capabilities& c = settings::kCapabilities);

}  // namespace est
}  // namespace power
