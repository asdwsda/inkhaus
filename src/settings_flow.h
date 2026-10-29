#pragma once
// Holds the open Settings screen (task 022 logic) and its 60 s idle timer. No drawing and no
// storage here: main draws with render::drawSettings and saves with settings_nvs.
#include "settings_ui.h"

namespace settings_flow {

struct Result {
  bool redraw = false;   // something visible changed
  bool refresh = false;  // "Refresh now": the caller starts a state fetch, then calls refreshDone()
  bool pageChanged = false;  // main <-> Tools: draw the new page with a full refresh
  bool toolsOpened = false;  // ... and it is the Tools page (probe the SD card, setSdPresent(), then draw)
  bool left = false;     // the screen closed; `leave` says what to save and apply
  settings_ui::LeaveResult leave;
};

bool active();
void open(const settings::Settings &saved, settings::Orientation current, bool ringVisible, uint32_t now);
const settings_ui::SettingsScreen &screen();

void setSdPresent(bool present);  // when the Tools page opens (Debug log note variant)

Result onTouch(ui::Point p, uint32_t now);
Result onKey(ui::LogicalKey key, uint32_t now);
Result tick(uint32_t now);  // idle timeout
Result refreshDone(bool ok);

}  // namespace settings_flow
