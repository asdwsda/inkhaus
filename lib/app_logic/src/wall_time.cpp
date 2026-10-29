#include "wall_time.h"

#include <cstdio>

namespace app_logic {

namespace {
// Howard Hinnant's days_from_civil / civil_from_days.
int64_t daysFromCivil(int64_t y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}
void civilFromDays(int64_t z, int &y, int &m, int &d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yy = yoe + era * 400;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  d = int(doy - (153 * mp + 2) / 5 + 1);
  m = int(mp < 10 ? mp + 3 : mp - 9);
  y = int(yy + (m <= 2));
}
}  // namespace

bool shouldSyncNtp(int64_t lastSyncEpoch, int64_t nowEpoch, bool rtcLowVoltage) {
  if (rtcLowVoltage || !plausibleEpoch(nowEpoch) || !plausibleEpoch(lastSyncEpoch)) return true;
  // A last sync in the future (clock went backwards) cannot be trusted either.
  if (lastSyncEpoch > nowEpoch) return true;
  return nowEpoch - lastSyncEpoch > kNtpMaxAgeS;
}

Civil epochToCivil(int64_t epoch_s, int32_t utc_offset_s) {
  const int64_t t = epoch_s + utc_offset_s;
  int64_t days = t / 86400;
  int64_t rem = t % 86400;
  if (rem < 0) { rem += 86400; --days; }
  Civil c;
  civilFromDays(days, c.year, c.month, c.day);
  c.hour = int(rem / 3600);
  c.minute = int(rem % 3600 / 60);
  c.second = int(rem % 60);
  return c;
}

int64_t civilToEpoch(const Civil &c) {
  return daysFromCivil(c.year, c.month, c.day) * 86400 + c.hour * 3600 + c.minute * 60 + c.second;
}

Label refreshLabel(int64_t lastOk, int64_t now, int32_t off, int refreshMin, bool failed, bool landscape) {
  Label l;
  const bool haveClock = plausibleEpoch(now);
  const int64_t limitS = refreshMin > 0 ? int64_t(refreshMin) * 60 * 2 : 3 * 3600;
  if (!plausibleEpoch(lastOk)) {
    l.stale = failed;
    if (landscape) { l.line1 = "updated"; l.line2 = "never"; }
    else l.line1 = "not updated yet";
    return l;
  }
  l.stale = failed || (haveClock && now - lastOk > limitS);
  const Civil c = epochToCivil(lastOk, off);
  char hm[8];
  snprintf(hm, sizeof hm, "%02d:%02d", c.hour, c.minute);
  bool sameDay = true;
  char dm[12] = "";
  if (haveClock) {
    const Civil n = epochToCivil(now, off);
    sameDay = n.year == c.year && n.month == c.month && n.day == c.day;
  }
  if (!sameDay) snprintf(dm, sizeof dm, "%02d.%02d.", c.day, c.month);
  if (landscape) {
    if (sameDay) { l.line1 = "updated"; l.line2 = hm; }
    else { l.line1 = dm; l.line2 = hm; }
  } else {
    l.line1 = std::string("updated ") + (sameDay ? "" : std::string(dm) + " ") + hm;
  }
  return l;
}

Label statusLabel(Phase phase, int64_t lastOk, int64_t now, int32_t off, int refreshMin, bool failed,
                  bool landscape) {
  Label l;
  auto two = [&](const char *a, const char *b, const char *whole) {
    if (landscape) { l.line1 = a; l.line2 = b; }
    else l.line1 = whole;
  };
  const bool have = plausibleEpoch(lastOk);
  switch (phase) {
    case Phase::ConnectingWifi:
      two("Wi-Fi", "joining", "connecting Wi-Fi");
      l.stale = have && refreshLabel(lastOk, now, off, refreshMin, failed, landscape).stale;
      return l;
    case Phase::Fetching:
      two("fetching", "states", "fetching states");
      l.stale = have && refreshLabel(lastOk, now, off, refreshMin, failed, landscape).stale;
      return l;
    case Phase::Demo: two("demo", "mode", "demo mode"); return l;
    case Phase::Online: return refreshLabel(lastOk, now, off, refreshMin, failed, landscape);
    case Phase::AuthError: two("token", "rejected", "token rejected"); l.stale = have; return l;
    case Phase::Offline: {
      l.stale = have;
      if (!have) {
        if (landscape) { l.line1 = "offline"; l.line2 = "never"; }
        else l.line1 = "offline";
        return l;
      }
      const Civil c = epochToCivil(lastOk, off);
      char hm[8];
      snprintf(hm, sizeof hm, "%02d:%02d", c.hour, c.minute);
      bool sameDay = true;
      if (plausibleEpoch(now)) {
        const Civil n = epochToCivil(now, off);
        sameDay = n.year == c.year && n.month == c.month && n.day == c.day;
      }
      char dm[12] = "";
      if (!sameDay) snprintf(dm, sizeof dm, "%02d.%02d.", c.day, c.month);
      if (landscape) { l.line1 = "offline"; l.line2 = sameDay ? hm : dm; }
      else l.line1 = std::string("offline, last ") + (sameDay ? "" : std::string(dm) + " ") + hm;
      return l;
    }
  }
  return l;
}

}  // namespace app_logic
