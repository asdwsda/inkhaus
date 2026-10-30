#include "stats_plan.h"

namespace history {

StatsPeriod linePeriod(int hours) {
  if (hours <= 24) return StatsPeriod::FiveMinute;
  if (hours <= 168) return StatsPeriod::Hour;
  return StatsPeriod::Day;
}

StatsPeriod barPeriod(BarPeriodCfg cfg, int hours) {
  if (cfg == BarPeriodCfg::Hour) return StatsPeriod::Hour;
  if (cfg == BarPeriodCfg::Day) return StatsPeriod::Day;
  return hours <= 48 ? StatsPeriod::Hour : StatsPeriod::Day;
}

Source chooseSource(SourceCfg cfg, bool binary, bool bar, int hours) {
  if (bar) return Source::Statistics;
  if (cfg == SourceCfg::History) return Source::History;
  if (cfg == SourceCfg::Statistics) return Source::Statistics;
  return (binary || hours <= 24) ? Source::History : Source::Statistics;
}

BarWindow barWindow(int64_t now, int hours, int64_t bs, int32_t utcOffsetS) {
  BarWindow w;
  if (bs < 1) bs = 3600;
  int64_t span = int64_t(hours < 1 ? 1 : hours) * 3600;
  int64_t n = (span + bs - 1) / bs;
  if (n < 1) n = 1;
  if (n > kMaxBuckets) n = kMaxBuckets;
  w.count = int(n);
  // Buckets start at local midnight/hour boundaries (HA), so align in local time. The offset at `now` is used
  // for the whole window (a DST change inside the window shifts the older buckets by an hour).
  const int64_t local = now + utcOffsetS;
  int64_t floorLocal = (local / bs) * bs;
  if (local < 0 && floorLocal > local) floorLocal -= bs;
  w.start = floorLocal - utcOffsetS - (n - 1) * bs;
  w.end = now;
  return w;
}

}  // namespace history
