#pragma once
// CSV formatting of the wake log (task 029). Pure; the SD access is in src/wakelog.cpp.
#include <stdint.h>

#include <string>

namespace app_logic {

// -1 in a duration means "did not happen in this wake" (empty CSV field).
struct WakeRecord {
  int64_t epoch = 0;            // wall clock at sleep entry; 0 = clock not set
  std::string wake;             // wake reason name
  std::string session;          // boot / interactive / scheduled / low_battery
  int32_t battery_mv = 0;
  int32_t awake_ms = 0;         // total, boot to sleep entry
  int32_t display_ms = -1;      // boot until the display is up
  int32_t wifi_ms = -1;         // session start until Wi-Fi connected
  int32_t fetch_ms = -1;        // Wi-Fi connected until the first fetch result (incl. TLS)
  int32_t refresh_ms = -1;      // time spent in display refreshes
  std::string fetch_result;     // ok / fail / none
  int32_t refreshes = 0;        // number of refreshes
  std::string reset_reason;     // esp_reset_reason() name
  std::string wake_cause;       // esp_sleep_get_wakeup_cause() name
  std::string ext1_mask;        // hex of esp_sleep_get_ext1_wakeup_status()
  int32_t rtc_boot_count = 0;   // RTC counter, +1 per boot; 1 after a power loss
  std::string slept_path;       // sleep path used before this boot (RTC memory)
};

constexpr size_t kWakeLogMaxBytes = 1024 * 1024;

// Header line, with the trailing newline.
std::string wakeLogHeader();
// One data line, with the trailing newline. Text fields are reduced to [A-Za-z0-9_.-] (no commas).
std::string wakeLogLine(const WakeRecord &r);

}  // namespace app_logic
