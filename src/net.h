#pragma once
// Wi-Fi connection with BSSID/channel (+ DHCP lease) cache. Non-blocking.
// Usage: net::begin(cfg.connection.wifi); loop: net::poll(); on sleep: net::powerDown().
#include <string>
#include <vector>

#include "config_model.h"

namespace net {

enum class State { Idle, Connecting, Connected, Failed };

// Task 047: the profile list from secrets.yaml and the two Settings (wifi_profile: 0 = Auto, else
// 1-based; ha_link: settings::HaLink as int). Call before begin(); stays in effect until called again.
// Without it, begin() uses the single profile it is given.
void configure(const std::vector<config::WifiConfig> &profiles, int forcedProfile, int haLink);

// Starts connecting (returns at once). Restarts if already running.
void begin(const config::WifiConfig &wifi);
// Drives the connection; call every loop iteration (cheap). Connected stays Connected
// until the link drops (then Failed) or stop()/powerDown().
State poll();
// Disconnects, keeps the radio initialised.
void stop();
// Wi-Fi off, modem sleep released. Call before sleep / power-off.
void powerDown();

// Forgets the cached DHCP lease (BSSID/channel stay), in RAM, RTC and NVS. Call when the
// host is unreachable although Wi-Fi is up. The next connect then runs DHCP.
void invalidateLease();

// The connected profile, read by the network task. profile -1 / ssidHash 0 = not connected.
struct LinkInfo {
  int profile = -1;
  uint32_t ssidHash = 0;  // netpolicy::ssidHash of the connected SSID (key for per-SSID memory)
  int haUrl = 0;          // config::HaUrlMode of the profile
  int haLink = 0;         // settings::HaLink from configure()
};
LinkInfo linkInfo();
std::string activeProfileName();  // "" if not connected (UI thread)

int rssi();          // dBm, 0 if not connected
int channel();       // 0 if not connected
uint32_t connectMs();  // duration of the last successful connect, 0 if none

}  // namespace net
