#pragma once
// Wi-Fi connection with BSSID/channel (+ DHCP lease) cache. Non-blocking.
// Usage: net::begin(cfg.connection.wifi); loop: net::poll(); on sleep: net::powerDown().
#include "config_model.h"

namespace net {

enum class State { Idle, Connecting, Connected, Failed };

// Starts connecting (returns at once). Restarts if already running.
void begin(const config::WifiConfig &wifi);
// Drives the connection; call every loop iteration (cheap). Connected stays Connected
// until the link drops (then Failed) or stop()/powerDown().
State poll();
// Disconnects, keeps the radio initialised.
void stop();
// Wi-Fi off, modem sleep released. Call before sleep / power-off.
void powerDown();

int rssi();          // dBm, 0 if not connected
int channel();       // 0 if not connected
uint32_t connectMs();  // duration of the last successful connect, 0 if none

}  // namespace net
