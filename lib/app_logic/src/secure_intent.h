#pragma once
// Session event for the alarm keypad (task 108): alarm_control_panel.alarm_* with the code. A custom call, so any
// fetch confirms it (task 036). The code is only in the call body; nothing here logs it.
#include <optional>
#include <string>

#include "session.h"

namespace app_logic {

std::optional<session::Event> alarmIntent(const std::string &panelId, AlarmCmd cmd, const std::string &code);

}  // namespace app_logic
