#pragma once
// Compact binary record of the last known entity states (task 037). Pure C++.
// Layout: "MPST", u8 format, i64 epoch, u16 count, entities, u32 CRC32 of everything before it.
// All little endian. A record is accepted whole or not at all.
#include <cstdint>
#include <string>
#include <vector>

#include "entity_state.h"

namespace statecodec {

constexpr uint8_t kFormat = 1;

std::string pack(const std::vector<EntityState>& states, int64_t epoch);
// False on any damage (magic, format, CRC, truncation, trailing bytes); outputs are untouched then.
bool unpack(const std::string& data, int64_t& epoch, std::vector<EntityState>& states);

// Keeps only the states whose id is in `ids` (config change: matching entities stay).
std::vector<EntityState> filterIds(const std::vector<EntityState>& states, const std::vector<std::string>& ids);

enum class Source { None, Rtc, Flash };
// Restore: the RTC record if valid, else the flash record, else nothing. A valid record that
// matches no configured entity does not count; the other source is tried then.
Source restore(const std::string& rtc, const std::string& flash, const std::vector<std::string>& ids,
               int64_t& epoch, std::vector<EntityState>& states);

}  // namespace statecodec
