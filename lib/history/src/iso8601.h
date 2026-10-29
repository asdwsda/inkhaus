#pragma once
// ISO-8601 timestamp -> Unix epoch seconds. Own routine: no strptime/mktime, so no TZ dependence.
#include <cstddef>
#include <cstdint>

namespace history {

// Parses "YYYY-MM-DDTHH:MM:SS[.fraction][Z|+HH:MM|-HH:MM|+HHMM]" (also ' ' instead of 'T').
// The fraction is dropped. A missing offset means UTC (HA sends UTC "+00:00").
// Returns false on any malformed input; `epoch` is then untouched.
bool parseIso8601(const char* s, size_t len, int64_t& epoch);

// Days since 1970-01-01 for a proleptic Gregorian date (month 1..12).
int64_t daysFromCivil(int y, unsigned m, unsigned d);

}  // namespace history
