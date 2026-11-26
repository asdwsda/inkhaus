#pragma once
// One drawing function per widget type (docs/ui/widgets.md revision 4).
#include <string>

#include "app_state.h"
#include "draw.h"
#include "ui_layout.h"

namespace render {

// Draws the widget (frame, content, badge) at placement.rect. innerSub > 0: that button has the
// rocker INNER focus border. stale: task 127 wake look (variant B) on the widgets that show an HA state.
void drawWidget(M5Canvas &c, const ui::Placement &pl, const uistate::WidgetData &data, const std::string &title,
                int innerSub = 0, bool stale = false);
// Task 127 (UNVERIFIED look): is this widget drawn in the greyed wake look while `stale` is on.
bool staleEligible(const uistate::WidgetData &data);

}  // namespace render
