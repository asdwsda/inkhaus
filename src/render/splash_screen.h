#pragma once
// Cold-boot splash (docs/ui/splash.md): logo, name, tagline, version, footer. Pure painting into the UI
// canvas, in the canvas' own orientation; the caller refreshes (one GC16 full refresh).
#include "draw.h"

namespace render {

constexpr const char *kAppName = "Inkhaus";

// `version` is the text shown as is ("v0.2.1"). Clears the canvas to PAPER first.
void drawSplash(M5Canvas &c, const char *version);

}  // namespace render
