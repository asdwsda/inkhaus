#pragma once
// Composes a whole frame into the canvas: status bar, tab bar, widgets, pager, focus ring, or the
// config error screen.
#include <string>

#include "app_state.h"
#include "draw.h"

namespace render {

// Link phase shown by the status bar (STYLE.md 5.3). None: nothing drawn (config error screen).
enum class Link { None, ConnectingWifi, Fetching, Online, Offline, AuthError, Demo };

struct StatusInfo {
  int batteryPct = 100;
  Link link = Link::None;
  // Refresh label (one line in portrait, two in landscape); outdated = clock icon after it (STYLE.md 5.3).
  std::string label1, label2;
  bool outdated = false;
  // Sleep badge (STYLE.md 5.5): "Zz" in a black rounded badge, drawn last, only in the frame that is on
  // the panel while the device sleeps. Region: ui::sleepBadgeRect(orientation).
  bool sleepBadge = false;
  bool mockLabel = false;  // test builds: draw the mockup's wifi icon and "updated 14:05" instead of "Demo"
};

void renderScreen(M5Canvas &c, const uistate::AppState &app, const StatusInfo &status);
// Just the sleep badge (what renderScreen draws last when status.sleepBadge is set), for the early-clear
// bookkeeping at wake (task 058).
void drawSleepBadgeOverlay(M5Canvas &c, settings::Orientation o);

}  // namespace render
