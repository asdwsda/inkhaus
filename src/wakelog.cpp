#include "wakelog.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config_sd.h"
#include "debuglog.h"
#include "wakelog_csv.h"

namespace wakelog {

namespace {

constexpr const char *kPath = "/wakelog.csv";
constexpr const char *kDebugPath = "/debug.log";
constexpr const char *kDebugOld = "/debug.old";
constexpr size_t kPendingMax = 4096;  // RAM buffer for lines not yet on the SD card

#ifdef WAKE_LOG_SD
constexpr bool kForced = true;
#else
constexpr bool kForced = false;
#endif
bool g_enabled = kForced;
std::string g_pending;

app_logic::WakeRecord g_rec;
uint32_t g_sessionStartMs = 0, g_wifiMs = 0;
bool g_sessionSeen = false, g_wifiSeen = false, g_fetchSeen = false;
uint32_t g_refreshMs = 0;
int g_refreshes = 0;

const char *sessionName(power::Session s) {
  switch (s) {
    case power::Session::Interactive: return "interactive";
    case power::Session::ScheduledRefresh: return "scheduled";
    default: return "low_battery";
  }
}

// Survive deep sleep; cleared by a power loss (then the count starts at 1 again).
RTC_DATA_ATTR uint32_t g_rtcBootCount = 0;
RTC_DATA_ATTR char g_rtcSleptPath[24] = "";

const char *resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

const char *causeName(esp_sleep_wakeup_cause_t c) {
  switch (c) {
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "UNDEFINED";
    case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
    case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
    case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
    case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
    case ESP_SLEEP_WAKEUP_ULP: return "ULP";
    case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
    case ESP_SLEEP_WAKEUP_UART: return "UART";
    default: return "OTHER";
  }
}

// Inside a mounted card: appends the pending debug lines to /debug.log, rotating at 1 MB.
void flushDebug() {
  if (g_pending.empty()) return;
  size_t size = 0;
  if (SD.exists(kDebugPath)) {
    File f = SD.open(kDebugPath, FILE_READ);
    if (f) {
      size = f.size();
      f.close();
    }
  }
  if (app_logic::debugLogNeedsRotation(size, g_pending.size())) {
    SD.remove(kDebugOld);
    SD.rename(kDebugPath, kDebugOld);
  }
  File f = SD.open(kDebugPath, SD.exists(kDebugPath) ? FILE_APPEND : FILE_WRITE);
  if (f) {
    f.print(g_pending.c_str());
    f.close();
  }
  g_pending.clear();
}

void appendLine(const std::string &line) {
  const uint32_t t0 = millis();
  if (!config_sd::mount()) {
    Serial.println("WAKELOG: no_card");
    return;
  }
  bool wrote = false;
  const char *note = "ok";
  {
    const bool exists = SD.exists(kPath);
    File f = SD.open(kPath, exists ? FILE_APPEND : FILE_WRITE);
    if (!f) {
      note = "open_failed";
    } else if (f.size() + line.size() + app_logic::wakeLogHeader().size() > app_logic::kWakeLogMaxBytes) {
      note = "full_not_appended";
    } else {
      if (f.size() == 0) f.print(app_logic::wakeLogHeader().c_str());
      wrote = f.print(line.c_str()) == line.size();
      if (!wrote) note = "write_failed";
    }
    if (f) f.close();
  }
  flushDebug();
  config_sd::unmount();
  Serial.printf("WAKELOG: result=%s wrote=%d sd_ms=%u\n", note, wrote ? 1 : 0, (unsigned)(millis() - t0));
}

}  // namespace

void setEnabled(bool on) {
  g_enabled = on || kForced;
  if (!g_enabled) g_pending.clear();
}

void note(const char *line) {
  if (!line) return;
  const std::string l(line);
  if (!app_logic::isDebugLogLine(l)) return;
  const std::string f = app_logic::formatDebugLogLine(millis(), l);
  if (g_pending.size() + f.size() <= kPendingMax) g_pending += f;
}

void logf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  Serial.print(buf);
  size_t n = strlen(buf);
  while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
  note(buf);
}

void setSleptPath(const char *path) {
  strncpy(g_rtcSleptPath, path ? path : "", sizeof g_rtcSleptPath - 1);
  g_rtcSleptPath[sizeof g_rtcSleptPath - 1] = 0;
}

void begin(const char *wakeName, power::Session session) {
  if (!g_enabled) return;
  g_rec = app_logic::WakeRecord();
  g_rec.wake = wakeName;
  g_rec.session = sessionName(session);
  g_rec.fetch_result = "none";
  g_rec.display_ms = int32_t(millis());  // boot::begin() (display start included) is done
  ++g_rtcBootCount;
  g_rec.reset_reason = resetName(esp_reset_reason());
  g_rec.wake_cause = causeName(esp_sleep_get_wakeup_cause());
  char mask[24];
  snprintf(mask, sizeof mask, "0x%llx", (unsigned long long)esp_sleep_get_ext1_wakeup_status());
  g_rec.ext1_mask = mask;
  g_rec.rtc_boot_count = int32_t(g_rtcBootCount);
  g_rec.slept_path = g_rtcSleptPath[0] ? g_rtcSleptPath : "none";
  g_rtcSleptPath[0] = 0;  // a restart without a sleep must not repeat the old path
  g_sessionSeen = g_wifiSeen = g_fetchSeen = false;
  g_refreshMs = 0;
  g_refreshes = 0;

  // Boot line: a boot that never reaches sleep is still logged.
  app_logic::WakeRecord b = g_rec;
  b.session = "boot";
  b.battery_mv = int32_t(M5.Power.getBatteryVoltage());
  b.awake_ms = int32_t(millis());
  appendLine(app_logic::wakeLogLine(b));
}

void sessionStarted(uint32_t nowMs) {
  if (!g_enabled) return;
  if (g_sessionSeen) return;
  g_sessionSeen = true;
  g_sessionStartMs = nowMs;
}

void wifiUp(uint32_t nowMs) {
  if (!g_enabled) return;
  if (!g_sessionSeen || g_wifiSeen) return;
  g_wifiSeen = true;
  g_wifiMs = nowMs;
  g_rec.wifi_ms = int32_t(nowMs - g_sessionStartMs);
}

void fetchDone(uint32_t nowMs, bool ok) {
  if (!g_enabled) return;
  if (g_fetchSeen) return;
  g_fetchSeen = true;
  g_rec.fetch_result = ok ? "ok" : "fail";
  g_rec.fetch_ms = int32_t(nowMs - (g_wifiSeen ? g_wifiMs : g_sessionStartMs));
}

void refreshed(uint32_t durationMs) {
  if (!g_enabled) return;
  ++g_refreshes;
  g_refreshMs += durationMs;
}

void append(power::Session session, int64_t epoch, int32_t batteryMv) {
  if (!g_enabled) return;
  g_rec.session = sessionName(session);
  g_rec.epoch = epoch;
  g_rec.battery_mv = batteryMv;
  g_rec.refreshes = g_refreshes;
  g_rec.refresh_ms = g_refreshes ? int32_t(g_refreshMs) : -1;
  g_rec.awake_ms = int32_t(millis());  // before the SD access: its cost is not part of the wake
  const std::string line = app_logic::wakeLogLine(g_rec);

  appendLine(line);
}

bool handleLine(const std::string &line) {
  const char *path = line == "wakelog" ? kPath : line == "debuglog" ? kDebugPath : nullptr;
  if (!path) return false;
  if (!config_sd::mount()) {
    Serial.print("WAKELOG-ERR no_card\n");
    return true;
  }
  {
    File f = SD.open(path, FILE_READ);
    if (!f) {
      Serial.print("WAKELOG-ERR no_file\n");
    } else {
      Serial.printf("WAKELOG-BEGIN %u\n", (unsigned)f.size());
      uint8_t buf[512];
      int n;
      while ((n = f.read(buf, sizeof buf)) > 0) Serial.write(buf, size_t(n));
      Serial.print("\nWAKELOG-END\n");
    }
    if (f) f.close();
  }
  config_sd::unmount();
  return true;
}

}  // namespace wakelog
