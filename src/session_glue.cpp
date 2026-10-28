#include "session_glue.h"

#include <M5Unified.h>
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

void ntpPoll(bool wifiUp) {
  static enum { Idle, Waiting, Done } st = Idle;
  static uint32_t t0 = 0;
  if (!wifiUp) {
    if (st == Waiting) st = Idle;  // link lost: try again next session
    return;
  }
  if (st == Idle) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    t0 = millis();
    st = Waiting;
    Serial.println("APP: ntp started");
  } else if (st == Waiting) {
    const int64_t t = (int64_t)time(nullptr);
    if (app_logic::plausibleEpoch(t)) {
      const app_logic::Civil c = app_logic::epochToCivil(t, 0);
      m5::rtc_datetime_t dt;
      dt.date.year = c.year; dt.date.month = c.month; dt.date.date = c.day;
      dt.time.hours = c.hour; dt.time.minutes = c.minute; dt.time.seconds = c.second;
      M5.Rtc.setDateTime(dt);
      Serial.printf("APP: ntp synced ms=%u\n", (unsigned)(millis() - t0));
      st = Done;
    } else if (millis() - t0 > 15000) {
      Serial.println("APP: ntp timeout");
      st = Done;
    }
  }
}

}  // namespace session_glue
