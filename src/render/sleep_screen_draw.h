#pragma once
// Paints the sleep screen (task 101, docs/ui/sleep-screen.md) from a composed view (lib/sleep_screen). Clears the
// canvas to PAPER first; the caller refreshes (one GC16 full refresh). UNVERIFIED on the device until looked at.
#include "draw.h"
#include "sleep_screen.h"

namespace render {

void drawSleepScreen(M5Canvas &c, const sleepscreen::View &v);

}  // namespace render
