#pragma once
// Local fake actions for the demo: what the entities would look like after a tap (as if Home
// Assistant had confirmed it). Pure logic.
#include <vector>

#include "config_model.h"
#include "entity_state.h"

namespace uistate {

struct ActionResult {
  bool changed = false;
  std::vector<EntityState> states;  // new states of the affected entities
};

// What a tap on a light/switch/automation widget asks Home Assistant to do, from the state the
// user sees (never a toggle): TurnOn if the first available member is not "on", else TurnOff.
// None for other widget types or if no member is available.
enum class ToggleIntent { None, TurnOn, TurnOff };
ToggleIntent toggleIntent(config::WidgetType type, const std::vector<const EntityState *> &members);

// type/sub: widget type and sub-region id (0 for single-action widgets; cover/vacuum 1..3,
// climate 1 = minus, 2 = plus). members: the widget's entities (nullptr = not loaded).
// Nothing changes for unavailable entities, or a climate target at its limit.
ActionResult fakeAction(config::WidgetType type, int sub, const std::vector<const EntityState *> &members);

}  // namespace uistate
