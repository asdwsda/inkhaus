#pragma once
// Plausible fake entity states for the demo (no Home Assistant). Pure logic.
#include <string>
#include <vector>

#include "config_model.h"
#include "entity_state.h"

namespace uistate {

// One state per entity id used by the config, matching the approved mockups where the ids of
// config.example.yaml are used, and a plausible default per domain for any other id.
std::vector<EntityState> demoStates(const config::Config &cfg);

// "living_room_floor_lamp" -> "Living room floor lamp".
std::string prettyName(const std::string &entityId);

}  // namespace uistate
