#pragma once
// The history popup chrome drawn over the tab screen (docs/ui/history.md, task 053): shadow, panel, title row,
// close button, current value, note line, and the state messages (Loading, No data, Too many points, Not
// available). The graphs themselves are drawn by task 052's renderer (see the hook in popup_history.cpp).
#include "app_state.h"
#include "draw.h"

namespace render {

// Draws the popup if it is visible; does nothing otherwise.
void drawHistoryPopup(M5Canvas &c, const uistate::AppState &app);

}  // namespace render
