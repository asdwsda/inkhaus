#pragma once
// Media player widget (wide) and its popup (task 106, docs/ui/widgets-new.md section 4). Own file so the
// other new widgets (tasks 103-109) merge without touching it.
#include <string>

#include "app_state.h"
#include "draw.h"

namespace render {

// innerSub: rocker INNER focus sub-region id (1 prev, 2 play/pause, 3 next, 4 text block), 0 = none.
void drawMediaWidget(M5Canvas &c, const ui::Placement &pl, const uistate::WidgetData &d, const std::string &title,
                     int innerSub);

// The popup drawn over the tab screen if open; does nothing otherwise.
void drawMediaPopup(M5Canvas &c, const uistate::AppState &app);

}  // namespace render
