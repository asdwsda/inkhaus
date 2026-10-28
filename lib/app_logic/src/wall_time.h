#pragma once
// Wall-clock helpers (UTC epoch seconds <-> civil time) and the status-bar label text. Pure.
#include <cstdint>
#include <string>

namespace app_logic {

struct Civil {
  int year = 1970, month = 1, day = 1, hour = 0, minute = 0, second = 0;
};

constexpr int64_t kMinPlausibleEpoch = 1700000000;  // below this the RTC counts as unset
inline bool plausibleEpoch(int64_t e) { return e >= kMinPlausibleEpoch; }

// NTP policy (ARCHITECTURE.md 2026-10-28): sync when the clock is untrusted (never synced, RTC low-voltage flag,
// implausible time) or the last confirmed sync is older than 24 h. lastSyncEpoch 0 = never; nowEpoch 0 = unset.
constexpr int64_t kNtpMaxAgeS = 24 * 3600;
bool shouldSyncNtp(int64_t lastSyncEpoch, int64_t nowEpoch, bool rtcLowVoltage);

// Local civil time of an epoch (UTC) plus utc_offset_s.
Civil epochToCivil(int64_t epoch_s, int32_t utc_offset_s = 0);
// Inverse for UTC civil time (offset 0). Values are not range-checked beyond the calendar maths.
int64_t civilToEpoch(const Civil &c);

// Status-bar refresh label (docs/ui/STYLE.md 5.3). Two lines for landscape, one for portrait.
struct Label {
  std::string line1, line2;  // portrait: line1 only
  bool stale = false;
};
// lastOk: epoch of the last successful fetch (0 = never); nowEpoch 0/implausible = unknown clock.
// Stale: age > 2 x refresh_interval (3 h if off), or the last fetch failed.
Label refreshLabel(int64_t lastOkEpoch, int64_t nowEpoch, int32_t utcOffsetS, int refreshIntervalMin,
                   bool lastFetchFailed, bool landscape);

}  // namespace app_logic
