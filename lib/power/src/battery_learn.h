#pragma once
// Learned low-battery limit (task 079). Pure logic, no Arduino includes.
// A brownout (or crash) on battery at some voltage says "the cell cannot deliver the Wi-Fi spike below here".
// limit = mean(event mV of the current and previous charge cycle) + kMarginMv, clamped to [kMinLimitMv,
// kMaxLimitMv]. An eventless charge cycle lowers the limit by kDecayMv (floor kMinLimitMv). A charge cycle ends
// when a real discharge was stored (0 < last_mv < kBelowMv) and >= kFullMv is seen on two readings
// >= kFullHoldS apart (a short USB plug-in does not count).
#include <cstdint>

#include "settings.h"

namespace power {

constexpr int32_t kMinLimitMv = 3300;
constexpr int32_t kMaxLimitMv = 3900;
constexpr int32_t kMarginMv = 50;
constexpr int32_t kDecayMv = 50;
constexpr int32_t kFullMv = 4150;
constexpr int32_t kBelowMv = 3900;
constexpr int kRecentCycles = 2;
constexpr int32_t kFullHoldS = 300;
constexpr int32_t kCrashSlackMv = 30;  // crash counts only if boot mV <= last mV + this (else USB inferred)
constexpr int kMaxEvents = 8;
constexpr int32_t kCrashMaxMv = 3950;  // a crash above this is not a battery event (probably on USB)
constexpr int32_t kStoreBelowMv = 4000;
constexpr int32_t kStoreDropMv = 20;

enum class ResetKind { Other, Brownout, Crash };  // Crash = TASK_WDT / PANIC / other watchdogs

struct BatteryEvent {
    int32_t mv = 0;
    int32_t cycle = 0;
};

struct BatteryLearn {
    int32_t limit_mv = 0;  // 0 = nothing learned yet
    int32_t cycle = 0;     // charge cycles counted so far
    int32_t last_mv = 0;   // last stored battery mV (NVS copy), 0 = none
    int32_t full_since_s = -1;  // runtime only: time of the first reading >= kFullMv, -1 = none
    int n_events = 0;      // newest last
    BatteryEvent events[kMaxEvents];
    bool operator==(const BatteryLearn& o) const;
};

// Limit in force: the learned one when learning is on and a value exists, else kMinLimitMv.
int32_t effective_limit(const BatteryLearn& st, bool learning_on);
inline bool is_learned(const BatteryLearn& st, bool learning_on) {
    return learning_on && st.limit_mv > kMinLimitMv;
}

// Does this reset count as an event given the last battery mV seen before it (0 = unknown) and the reading
// at boot (0 = unknown)? Crashes need boot_mv <= last_mv + kCrashSlackMv.
bool is_battery_event(ResetKind k, int32_t last_mv, int32_t boot_mv);
// Records an event at the current cycle and recomputes the limit (mean of recent events + margin). mv <= 0 is ignored.
void record_event(BatteryLearn& st, int32_t mv);
// Feed battery samples with a monotonic clock (seconds). Tracks charge cycles; at the end of a cycle without
// an event the limit decays. Returns true when the persisted state changed (cycle end).
bool on_sample(BatteryLearn& st, int32_t mv, int32_t now_s);

// Last-mV bookkeeping: write to NVS only when below kStoreBelowMv and >= kStoreDropMv under the stored value.
bool should_store_last_mv(int32_t stored_mv, int32_t mv);
// RTC copy (valid only with its magic) wins over the NVS copy.
int32_t pick_last_mv(bool rtc_valid, int32_t rtc_mv, int32_t nvs_mv);

// Percent with 0 % at empty_mv (the learned limit), the default curve stretched over [empty_mv, 4200].
int battery_percent_scaled(int32_t mv, int32_t empty_mv);

// Persistence through the settings key-value interface (namespace chosen by the store).
BatteryLearn load_learn(const settings::KeyValueStore& store);
void save_learn(settings::KeyValueStore& store, const BatteryLearn& st);

}  // namespace power
