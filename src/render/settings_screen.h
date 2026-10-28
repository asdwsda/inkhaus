#pragma once
// Draws the Settings screen (docs/ui/settings.md) into the UI canvas. All geometry and texts come
// from lib/settings_ui; this file only paints. No refresh here: the caller batches display().
#include <string>

#include "draw.h"
#include "settings_ui.h"

namespace render {

// Read-only values of the info block, snapshotted by the caller when the screen opens.
struct SettingsInfo {
  std::string firmware = "v0.0.0";
  settings_ui::BatteryInfo battery;
  settings_ui::HaInfo ha;
  settings_ui::WifiInfo wifi;
  settings_ui::ConfigInfo config;
  settings_ui::WallTime now;
};

// Whole screen: clears the canvas to PAPER, draws everything for the screen's layout orientation,
// the unsaved chip when dirty, the focus ring when screen.ringVisible().
void drawSettings(M5Canvas &c, const settings_ui::SettingsScreen &screen, const SettingsInfo &info);

}  // namespace render
