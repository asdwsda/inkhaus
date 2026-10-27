#pragma once
#include <map>
#include <string>
#include <vector>

#include "entity_state.h"

class StateStore {
 public:
  // Stores the given states and returns the ids whose state changed (new ids
  // count as changed). Order follows the input order, without duplicates.
  std::vector<std::string> update(const std::vector<EntityState>& states);

  const EntityState* get(const std::string& id) const;
  size_t size() const { return states_.size(); }

 private:
  std::map<std::string, EntityState> states_;
};
