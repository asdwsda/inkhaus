#include "wakelog.h"

#ifdef WAKE_LOG_SD

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#include "config_sd.h"
#include "wakelog_csv.h"

namespace wakelog {

namespace {

constexpr const char *kPath = "/wakelog.csv";

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

}  // namespace

void begin(const char *wakeName, power::Session session) {
  g_rec = app_logic::WakeRecord();
  g_rec.wake = wakeName;
  g_rec.session = sessionName(session);
  g_rec.fetch_result = "none";
  g_rec.display_ms = int32_t(millis());  // boot::begin() (display start included) is done
  g_sessionSeen = g_wifiSeen = g_fetchSeen = false;
  g_refreshMs = 0;
  g_refreshes = 0;
}

void sessionStarted(uint32_t nowMs) {
  if (g_sessionSeen) return;
  g_sessionSeen = true;
  g_sessionStartMs = nowMs;
}

void wifiUp(uint32_t nowMs) {
  if (!g_sessionSeen || g_wifiSeen) return;
  g_wifiSeen = true;
  g_wifiMs = nowMs;
  g_rec.wifi_ms = int32_t(nowMs - g_sessionStartMs);
}

void fetchDone(uint32_t nowMs, bool ok) {
  if (g_fetchSeen) return;
  g_fetchSeen = true;
  g_rec.fetch_result = ok ? "ok" : "fail";
  g_rec.fetch_ms = int32_t(nowMs - (g_wifiSeen ? g_wifiMs : g_sessionStartMs));
}

void refreshed(uint32_t durationMs) {
  ++g_refreshes;
  g_refreshMs += durationMs;
}

void append(power::Session session, int64_t epoch, int32_t batteryMv) {
  g_rec.session = sessionName(session);
  g_rec.epoch = epoch;
  g_rec.battery_mv = batteryMv;
  g_rec.refreshes = g_refreshes;
  g_rec.refresh_ms = g_refreshes ? int32_t(g_refreshMs) : -1;
  g_rec.awake_ms = int32_t(millis());  // before the SD access: its cost is not part of the wake
  const std::string line = app_logic::wakeLogLine(g_rec);

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
  config_sd::unmount();
  Serial.printf("WAKELOG: result=%s wrote=%d sd_ms=%u\n", note, wrote ? 1 : 0, (unsigned)(millis() - t0));
}

bool handleLine(const std::string &line) {
  if (line != "wakelog") return false;
  if (!config_sd::mount()) {
    Serial.print("WAKELOG-ERR no_card\n");
    return true;
  }
  {
    File f = SD.open(kPath, FILE_READ);
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

#endif  // WAKE_LOG_SD
