#pragma once
#include <cstdint>
#include <string>

// Path+query of the HA REST history call for one entity (task 051, design 046 section 1):
//   /api/history/period/<start>?filter_entity_id=<id>&end_time=<end>&minimal_response
//   &no_attributes&significant_changes_only
// Times are Unix epoch seconds, sent as UTC ISO-8601 ("+00:00", percent-encoded).
// Returns false (out untouched) for an empty/odd entity id or end <= start.
bool build_history_path(const std::string& entity_id, int64_t start_epoch, int64_t end_epoch,
                        std::string& out);

// "2026-10-29T08:00:00+00:00" (not encoded).
std::string iso_utc(int64_t epoch);
