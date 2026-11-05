#pragma once
// Alarm panel, lock and garage door widgets (1x1), the alarm keypad popup and the confirm dialog
// (task 108, docs/ui/widgets-new.md sections 5-7). Own file so the other new widgets merge cleanly.
// UNVERIFIED (device): the whole look; written from the spec numbers, not yet seen on the panel.
#include "app_state.h"
#include "draw.h"

namespace render {

// The widget at `r`: toggle layout (name F24 cy 30, icon M, state F24), INK fill when it needs attention.
void drawSecureWidget(M5Canvas &c, const ui::Rect &r, const uistate::WidgetData &d, const std::string &title);
// The keypad popup (over the tab screen) and the confirm dialog (above everything); nothing while closed.
void drawAlarmPopup(M5Canvas &c, const uistate::AppState &app);
void drawConfirmDialog(M5Canvas &c, const uistate::AppState &app);
// The dialog itself (also used by the notifications "Dismiss all?" confirm).
void drawConfirmPanel(M5Canvas &c, settings::Orientation o, const std::string &title);

}  // namespace render
