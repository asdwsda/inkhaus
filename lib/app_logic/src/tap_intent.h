#pragma once
// Turns a tap's local (optimistic) result into the session event that asks Home Assistant for it.
// The UI state computes the requested result for a tap (uistate::fakeAction); in real mode the
// same result is sent as an explicit action (never a toggle).
#include <optional>
#include <vector>

#include "config_model.h"
#include "entity_state.h"
#include "session.h"

namespace app_logic {

// type/sub as in ui::Hit (sub 0 for single-action widgets). requested: the pending states.
// Returns nothing for widgets without an action or empty input.
std::optional<session::Event> intentFromRequest(config::WidgetType type, int sub,
                                                const std::vector<EntityState> &requested);

}  // namespace app_logic
