#pragma once
#include <string>
#include <vector>

#include "entity_state.h"
#include "template_builder.h"

struct ParseResult {
  bool ok = false;
  std::string error;
  std::vector<EntityState> states;  // one per requested id, same order
};

// Parses the output of the state template. Entities that are missing, have
// "s": null, or whose state is "unavailable" get available=false ("unknown" is a normal state, task 040).
ParseResult parse_state_response(const std::string& json,
                                 const std::vector<std::string>& requested_ids);

// Parses the response of POST /api/services/<domain>/<service>: a JSON array of the states that
// changed. Returns the entries whose entity_id is in wanted_ids (response order), typed with the
// same attribute rules as parse_state_response (only friendly_name, the domain's fixed attributes
// and `extra`). last_changed is converted from ISO 8601 to epoch seconds. ok=false on bad JSON.
ParseResult parse_service_response(const std::string& json,
                                   const std::vector<std::string>& wanted_ids,
                                   const ExtraAttrs& extra = ExtraAttrs());
