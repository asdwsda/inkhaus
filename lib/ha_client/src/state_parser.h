#pragma once
#include <string>
#include <vector>

#include "entity_state.h"

struct ParseResult {
  bool ok = false;
  std::string error;
  std::vector<EntityState> states;  // one per requested id, same order
};

// Parses the output of the state template. Entities that are missing, have
// "s": null, or whose state is "unavailable"/"unknown" get available=false.
ParseResult parse_state_response(const std::string& json,
                                 const std::vector<std::string>& requested_ids);
