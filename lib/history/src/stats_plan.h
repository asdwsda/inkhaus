#pragma once
// Pure choices for the statistics path (task 054): period and data source.
#include <cstdint>

#include "stats_types.h"

namespace history {

// Line chart: hours <= 24 -> 5minute, <= 168 -> hour, else day.
StatsPeriod linePeriod(int hours);

enum class BarPeriodCfg : uint8_t { Auto, Hour, Day };
// Bar chart: configured period; Auto = hour for <= 48 h, else day.
StatsPeriod barPeriod(BarPeriodCfg cfg, int hours);

enum class SourceCfg : uint8_t { Auto, History, Statistics };
enum class Source : uint8_t { History, Statistics };
// Auto: history for binary entities or hours <= 24, statistics otherwise. Bars always use
// statistics. An explicit History / Statistics is kept (except for bars).
Source chooseSource(SourceCfg cfg, bool binary, bool bar, int hours);

}  // namespace history
