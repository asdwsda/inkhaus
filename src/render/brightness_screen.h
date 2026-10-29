#pragma once
// The brightness popup drawn over the tab screen (docs/ui/brightness.md, task 045).
#include "app_state.h"
#include "draw.h"

namespace render {

// Draws the panel with its shadow if the popup is open; does nothing otherwise.
void drawBrightnessPopup(M5Canvas &c, const uistate::AppState &app);

}  // namespace render
