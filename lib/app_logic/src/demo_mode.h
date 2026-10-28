#pragma once
// Demo mode (task 034): which config source to load and whether Wi-Fi / Home Assistant sessions
// run. Pure decisions, no Arduino.
#include "settings.h"

namespace app_logic {

enum class ConfigSource {
  Card,      // /config.yaml (+ secrets.yaml) from the SD card; the demo only when the card has none
  Embedded,  // demo.yaml compiled into the firmware, whatever the card holds
};

ConfigSource chooseConfigSource(const settings::Settings &s);

// True: fake states and local fake actions, no Wi-Fi, no HA. Also for a config without a
// connection section, a compiled-in fallback demo and an error screen.
bool isDemoRun(const settings::Settings &s, bool cfgIsDemo, bool cfgHasConnection);

// Leaving Settings: the config source changed, so reload the config and redraw everything.
bool needsConfigReload(const settings::Settings &before, const settings::Settings &after);

}  // namespace app_logic
