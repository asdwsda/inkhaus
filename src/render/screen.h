#pragma once
// Composes a whole frame into the canvas: status bar, tab bar, widgets, pager, focus ring, or the
// config error screen.
#include <string>

#include "app_state.h"
#include "draw.h"

namespace render {

// Connection state shown by the status bar (STYLE.md 5.3). None: the demo label (or nothing).
enum class Link { None, Connecting, Online, Offline, AuthError };

// Sleep indicator (STYLE.md 5.5): drawn last, over the status bar (portrait: free space) or the tab strip
// (landscape), only in the frame that is on the panel while the device sleeps.
enum class SleepIndicator { None, Button, Touch };

struct StatusInfo {
  int batteryPct = 100;
  Link link = Link::None;
  // Refresh label (one line in portrait, two in landscape); stale = filled chip (STYLE.md 5.3).
  std::string label1, label2;
  bool stale = false;
  SleepIndicator sleepIndicator = SleepIndicator::None;
  bool mockLabel = false;  // test builds: draw the mockup's wifi icon and "updated 14:05" instead of "Demo"
};

void renderScreen(M5Canvas &c, const uistate::AppState &app, const StatusInfo &status);

}  // namespace render
