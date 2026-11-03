#include "http_date.h"

#include <cstring>

#include "wall_time.h"

namespace app_logic {

namespace {
const char *const kDays[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char *const kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool digits(const char *p, int n, int &out) {
  int v = 0;
  for (int i = 0; i < n; ++i) {
    if (p[i] < '0' || p[i] > '9') return false;
    v = v * 10 + (p[i] - '0');
  }
  out = v;
  return true;
}
bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
}  // namespace

bool parseHttpDate(const char *s, size_t n, int64_t &epoch) {
  if (!s || n != 29) return false;
  if (s[3] != ',' || s[4] != ' ' || s[7] != ' ' || s[11] != ' ' || s[16] != ' ' || s[19] != ':' ||
      s[22] != ':' || s[25] != ' ')
    return false;
  if (memcmp(s + 26, "GMT", 3) != 0) return false;
  int wd = -1, mon = -1;
  for (int i = 0; i < 7; ++i)
    if (memcmp(s, kDays[i], 3) == 0) wd = i;
  for (int i = 0; i < 12; ++i)
    if (memcmp(s + 8, kMonths[i], 3) == 0) mon = i + 1;
  if (wd < 0 || mon < 0) return false;
  int day, year, hh, mm, ss;
  if (!digits(s + 5, 2, day) || !digits(s + 12, 4, year) || !digits(s + 17, 2, hh) ||
      !digits(s + 20, 2, mm) || !digits(s + 23, 2, ss))
    return false;
  static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const int maxDay = dim[mon - 1] + (mon == 2 && leap(year) ? 1 : 0);
  if (year < 1970 || day < 1 || day > maxDay || hh > 23 || mm > 59 || ss > 59) return false;
  Civil c;
  c.year = year; c.month = mon; c.day = day; c.hour = hh; c.minute = mm; c.second = ss;
  const int64_t e = civilToEpoch(c);
  // 1970-01-01 was a Thursday (index 3 in kDays, Monday first).
  if ((int)(((e / 86400) + 3) % 7) != wd) return false;
  epoch = e;
  return true;
}

HttpDateDecision decideHttpDate(int64_t dateEpoch, int httpStatus, uint32_t rttMs, int64_t clockNow,
                                bool clockTrusted, int64_t lastSync) {
  HttpDateDecision d;
  if (httpStatus < 200 || httpStatus >= 300 || rttMs > kHttpDateMaxRttMs) return d;
  const int64_t est = dateEpoch + (500 + (int64_t)rttMs / 2 + 500) / 1000;
  if (!plausibleEpoch(est)) return d;
  d.accept = true;
  d.estimate = est;
  d.delta = clockNow > 0 ? d.estimate - clockNow : 0;
  const int64_t ad = d.delta < 0 ? -d.delta : d.delta;
  d.write = !clockTrusted || clockNow <= 0 || ad >= kHttpDateMinStepS;
  d.syncTime = d.write ? d.estimate : clockNow;
  d.saveSync = d.write || lastSync <= 0 || d.syncTime - lastSync >= kHttpDateSaveIntervalS;
  return d;
}

}  // namespace app_logic
