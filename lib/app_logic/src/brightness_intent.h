#pragma once
// Session event for a brightness popup send (task 045): light.turn_on with brightness_pct, or
// light.turn_off for 0. A custom call, so the session keeps the 036 behaviour: the widget stays
// optimistic until the call's response (or a confirmation fetch) shows the expected state.
#include <optional>
#include <string>

#include "session.h"

namespace app_logic {

std::optional<session::Event> brightnessIntent(const std::string &lightId, int pct);

}  // namespace app_logic
