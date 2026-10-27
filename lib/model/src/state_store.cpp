#include "state_store.h"

#include <algorithm>

std::vector<std::string> StateStore::update(const std::vector<EntityState>& states) {
  std::vector<std::string> changed;
  for (const EntityState& s : states) {
    auto it = states_.find(s.id);
    if (it == states_.end()) {
      states_.emplace(s.id, s);
    } else if (it->second != s) {
      it->second = s;
    } else {
      continue;
    }
    if (std::find(changed.begin(), changed.end(), s.id) == changed.end()) changed.push_back(s.id);
  }
  return changed;
}

const EntityState* StateStore::get(const std::string& id) const {
  auto it = states_.find(id);
  return it == states_.end() ? nullptr : &it->second;
}
