#pragma once
// One drawing function per widget type (docs/ui/widgets.md revision 4).
#include <string>

#include "app_state.h"
#include "draw.h"
#include "ui_layout.h"

namespace render {

// Draws the widget (frame, content, badge) at placement.rect. innerSub > 0: that button has the
// rocker INNER focus border.
void drawWidget(M5Canvas &c, const ui::Placement &pl, const uistate::WidgetData &data, const std::string &title,
                int innerSub = 0);

}  // namespace render
