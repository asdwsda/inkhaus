#pragma once
// Calendar widget (task 105, docs/ui/widgets-new.md section 3): next two events, read-only. Own file so the
// other new widgets (tasks 103-109) merge without touching it.
#include "app_state.h"
#include "draw.h"

namespace render {

void drawCalendarWidget(M5Canvas &c, const ui::Rect &r, const uistate::WidgetData &d);

}  // namespace render
