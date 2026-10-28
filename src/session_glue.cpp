#include "session_glue.h"

#include <M5Unified.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#include "net.h"
#include "wall_time.h"

namespace session_glue {

void ArduinoWifi::begin() { net::begin(cfg_); }

app_logic::IWifi::State ArduinoWifi::poll() {
  switch (net::poll()) {
    case net::State::Connected: return State::Connected;
    case net::State::Failed: return State::Failed;
    default: return State::Connecting;
  }
}

void ArduinoWifi::powerDown() { net::powerDown(); }

bool loadClockFromRtc() {
  if (!M5.Rtc.isEnabled()) return false;
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt)) return false;
  app_logic::Civil c{dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds};
  const int64_t e = app_logic::civilToEpoch(c);
  if (!app_logic::plausibleEpoch(e)) return false;
  timeval tv{(time_t)e, 0};
  settimeofday(&tv, nullptr);
  return true;
}

int64_t wallClock() {
  const int64_t t = (int64_t)time(nullptr);
  return app_logic::plausibleEpoch(t) ? t : 0;
}

namespace {
enum class NtpState { Off, Idle, Waiting, Done };
NtpState g_ntp = NtpState::Off;
uint32_t g_ntpT0 = 0;
int64_t g_ntpClock0 = 0;   // system clock when SNTP started (0 = unset)

int64_t loadLastSync() {
  Preferences p;
  if (!p.begin("clock", true)) return 0;
  const int64_t v = p.getLong64("ntp_ok", 0);
  p.end();
  return v;
}
void saveLastSync(int64_t t) {
  Preferences p;
  if (!p.begin("clock", false)) return;
  p.putLong64("ntp_ok", t);
  p.end();
}
}  // namespace

void ntpSessionStart() {
  const bool want = app_logic::shouldSyncNtp(loadLastSync(), wallClock(), M5.Rtc.getVoltLow());
  g_ntp = want ? NtpState::Idle : NtpState::Off;
  Serial.printf("APP: ntp %s\n", want ? "wanted" : "not_needed");
}

void ntpAbort() {
  if (g_ntp == NtpState::Idle || g_ntp == NtpState::Waiting) {
    Serial.println("APP: ntp abandoned (sleep)");
    sntp_stop();
    g_ntp = NtpState::Done;
  }
}

void ntpPoll(bool wifiUp) {
  if (g_ntp == NtpState::Off || g_ntp == NtpState::Done) return;
  if (!wifiUp) {
    if (g_ntp == NtpState::Waiting) g_ntp = NtpState::Idle;  // link lost: try again on reconnect
    return;
  }
  if (g_ntp == NtpState::Idle) {
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    g_ntpClock0 = wallClock();
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    g_ntpT0 = millis();
    g_ntp = NtpState::Waiting;
    Serial.println("APP: ntp started");
  } else if (g_ntp == NtpState::Waiting) {
    // The RTC-restored clock is plausible too: only a completed SNTP exchange counts.
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      const uint32_t ms = millis() - g_ntpT0;
      const int64_t t = (int64_t)time(nullptr);
      const app_logic::Civil c = app_logic::epochToCivil(t, 0);
      m5::rtc_datetime_t dt;
      dt.date.year = c.year; dt.date.month = c.month; dt.date.date = c.day;
      dt.time.hours = c.hour; dt.time.minutes = c.minute; dt.time.seconds = c.second;
      M5.Rtc.setDateTime(dt);
      if (g_ntpClock0 > 0)
        Serial.printf("APP: ntp synced ms=%u correction_s=%ld\n", (unsigned)ms,
                      (long)(t - (g_ntpClock0 + (int64_t)(ms / 1000))));
      else
        Serial.printf("APP: ntp synced ms=%u correction_s=unset\n", (unsigned)ms);
      saveLastSync(t);
      sntp_stop();
      g_ntp = NtpState::Done;
    } else if (millis() - g_ntpT0 > 15000) {
      Serial.println("APP: ntp timeout");
      sntp_stop();
      g_ntp = NtpState::Done;  // no retry: next session
    }
  }
}

}  // namespace session_glue
