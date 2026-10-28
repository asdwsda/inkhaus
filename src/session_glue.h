#pragma once
// Arduino parts of the session wiring: Wi-Fi adapter for the driver, wall clock (RTC + NTP).
#include <stdint.h>

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
// Call every loop iteration after Wi-Fi is up: starts NTP once, then writes the result to the RTC.
void ntpPoll(bool wifiUp);

}  // namespace session_glue
