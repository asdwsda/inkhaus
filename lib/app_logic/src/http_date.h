#pragma once
// HTTP Date header as a time source (task 097). Pure.
#include <cstddef>
#include <cstdint>

namespace app_logic {

// Parses exactly the IMF-fixdate form "Sun, 06 Nov 1994 08:49:37 GMT" (29 chars, UTC) into epoch seconds.
// Everything else (RFC 850, asctime, other zones, bad fields, wrong weekday, truncated, empty) is rejected.
// No exceptions, no allocation. `s` need not be NUL-terminated.
bool parseHttpDate(const char *s, size_t n, int64_t &epoch);

// A Date value is used only if the request round trip was <= kHttpDateMaxRttMs and the status is 2xx.
constexpr uint32_t kHttpDateMaxRttMs = 2000;
// The RTC is rewritten only if the clock is untrusted or the estimate differs by at least this many seconds.
constexpr int64_t kHttpDateMinStepS = 2;

struct HttpDateDecision {
  bool accept = false;     // the value is usable: counts as a sync for the NTP policy
  bool write = false;      // set the RTC / system clock to `estimate`
  int64_t estimate = 0;    // UTC epoch seconds "now"
  int64_t delta = 0;       // estimate - clockNow (0 if the clock is unset)
};

// Estimate = date + 0.5 s + rttMs/2, rounded to the nearest second. The server stamps the Date with 1 s
// truncation (mean error -0.5 s) and sent it about half a round trip before we read it.
// clockNow: current clock (0 = unset). clockTrusted: false when unset, implausible or the RTC low-voltage flag is set.
HttpDateDecision decideHttpDate(int64_t dateEpoch, int httpStatus, uint32_t rttMs, int64_t clockNow,
                                bool clockTrusted);

}  // namespace app_logic
