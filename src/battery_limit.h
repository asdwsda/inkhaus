#pragma once
// Device glue for the learned low-battery limit (task 079): NVS namespace "batlearn", last battery mV in
// RTC_NOINIT memory (and a throttled NVS copy). The logic lives in lib/power/battery_learn.
#include <cstdint>

namespace batterylimit {

// Call once in setup(), after the settings are loaded and before the session is chosen. Classifies the reset
// (BROWNOUT / TASK_WDT / PANIC on battery), records an event, saves, logs `battery_limit_mv=<n> learned=<0|1>`.
void begin(bool learning, int32_t battery_mv);
// Limit in force right now (3300 when learning is off or nothing learned).
int32_t limit(bool learning);
bool learned(bool learning);
// Feed the current battery mV (cheap; call at session start, before Wi-Fi, and every few seconds).
void sample(int32_t battery_mv);

}  // namespace batterylimit
