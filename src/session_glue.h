#pragma once
// Arduino parts of the session wiring: Wi-Fi adapter for the driver, wall clock (RTC + NTP).
#include <stdint.h>

#include <string>

#include "config_model.h"
#include "session_driver.h"

namespace session_glue {

// IWifi on top of net::begin/poll/powerDown (task 021).
class ArduinoWifi : public app_logic::IWifi {
 public:
  explicit ArduinoWifi(const config::WifiConfig &cfg) : cfg_(cfg) {}
  void begin() override;
  State poll() override;
  void powerDown() override;

 private:
  config::WifiConfig cfg_;
};

// Copies the RTC into the system clock if the RTC holds a plausible time. Returns true if it did.
bool loadClockFromRtc();
// UTC epoch seconds from the system clock (0 if unset).
int64_t wallClock();
// A new session starts: decides (app_logic::shouldSyncNtp, last sync from NVS) whether to sync in this session.
void ntpSessionStart();
// Call every loop iteration: if a sync is wanted and Wi-Fi is up, runs SNTP; the RTC and the NVS last-sync time
// are written only after a completed SNTP sync. One attempt per session (no retry; 15 s timeout).
void ntpPoll(bool wifiUp);
// Task 097: a successful HA reply carried this Date header. If usable (app_logic::decideHttpDate) the
// clock/RTC are set when needed, the NVS last-sync time is stored (no SNTP for 24 h) and a pending SNTP
// attempt is cancelled. Logs `TIME: http_date delta_s=N wrote=0|1`.
void httpDateObserved(const std::string &date, int status, uint32_t rttMs);
// The session wants to sleep: give up any pending sync (never delays sleep), try again next session.
void ntpAbort();

}  // namespace session_glue
