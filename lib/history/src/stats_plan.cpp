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

}  // namespace history
