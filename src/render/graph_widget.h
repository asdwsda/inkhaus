#pragma once
// A `display: graph` widget (task 071, docs/ui/history-widget.md): frame, name, current value and the history
// graph drawn by the popup's renderer (history_graph.h) with the widget geometry of lib/history.
#include "app_state.h"
#include "draw.h"
#include "ui_layout.h"

namespace render {

// Draws the widget at pl.rect. Only for placements with pl.graph set.
void drawGraphWidget(M5Canvas &c, const ui::Placement &pl, const uistate::AppState &app);

}  // namespace render
