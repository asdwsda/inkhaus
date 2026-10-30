#include "stats_types.h"

const char* stats_period_name(StatsPeriod p) {
  switch (p) {
    case StatsPeriod::FiveMinute: return "5minute";
    case StatsPeriod::Hour: return "hour";
    case StatsPeriod::Day: return "day";
  }
  return "hour";
}

const char* stats_type_name(StatsType t) {
  switch (t) {
    case StatsType::Mean: return "mean";
    case StatsType::Min: return "min";
    case StatsType::Max: return "max";
    case StatsType::Sum: return "sum";
    case StatsType::Change: return "change";
    case StatsType::State: return "state";
  }
  return "mean";
}

int64_t stats_period_seconds(StatsPeriod p) {
  switch (p) {
    case StatsPeriod::FiveMinute: return 300;
    case StatsPeriod::Hour: return 3600;
    case StatsPeriod::Day: return 86400;
  }
  return 3600;
}
