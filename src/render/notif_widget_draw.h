#pragma once
// Notifications widget (1x1) and its list popup + confirm dialog (task 103, docs/ui/widgets-new.md 1, 1.1, 7).
#include "app_state.h"
#include "draw.h"
#include "notif_widget.h"
#include "ui_layout.h"

namespace render {

// The 1x1 widget at `r`: bell, count, caption, fill when count > 0, dotted frame + warn badge when unavailable.
void drawNotifWidget(M5Canvas &c, const ui::Rect &r, const uistate::NotifView &v);
// The popup (and the confirm dialog above it); does nothing while closed.
void drawNotifPopup(M5Canvas &c, const uistate::AppState &app);

}  // namespace render
