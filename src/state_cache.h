#pragma once
// Last known entity states across sleep and power loss (task 037). Hardware side; the record
// format and the choice of source are in lib/model/state_codec.
// Log lines: "STATECACHE: key=value ...".
#include <stdint.h>

#include <string>
#include <vector>

#include "state_store.h"

namespace state_cache {

// Boot, after the app state is built and before the first render: fills `store` from RTC memory,
// else LittleFS (/states.bin), keeping only the entities in `ids`. Returns the epoch of the
// record (when the values were last confirmed by Home Assistant), 0 if nothing was restored.
int64_t restore(const std::vector<std::string> &ids, StateStore &store);

// Before sleep: writes the store to RTC memory and, when the states changed, to LittleFS.
// `okEpoch` is the time of the last successful fetch; 0 keeps the epoch of the restored record.
// An empty store never overwrites a record.
void save(const StateStore &store, int64_t okEpoch);

}  // namespace state_cache
