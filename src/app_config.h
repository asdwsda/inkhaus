#pragma once
// Where the UI config comes from: /config.yaml on the SD card, else the demo config compiled in
// from config.example.yaml. Test builds can inject another YAML (never reads or writes a real file):
//   -DTEST_CONFIG_BROKEN   a deliberately broken config (error screen)
//   -DTEST_CONFIG_MOCKUP   the approved mockup's tab layout (5 tabs, 10 widgets), for pixel diffs
#include <vector>

#include "config_model.h"
#include "demo_mode.h"

namespace app_config {

struct Loaded {
  config::Config cfg;
  bool demo = false;   // compiled-in demo config (no SD config), fake states
  bool error = false;  // show the error screen
  std::vector<config::ConfigError> errors;
  const char *source = "";  // for the log
};

// source Embedded (Demo mode setting): the compiled-in demo.yaml, SD card and cache untouched.
Loaded load(app_logic::ConfigSource source = app_logic::ConfigSource::Card);

// Drops the parsed-config cache in internal flash (call after /config.yaml was replaced).
void invalidateCache();

}  // namespace app_config
