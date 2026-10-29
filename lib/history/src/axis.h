#pragma once
// Y-axis scaling and ticks.
#include <vector>

namespace history {

struct Axis {
  double lo = 0, hi = 1;     // plotted range
  double step = 1;           // distance between ticks
  int decimals = 0;          // digits after the point that label the ticks exactly
  std::vector<double> ticks; // multiples of `step` inside [lo, hi], ascending
  // Position of v in the plot, 0 (lo) .. 1 (hi), clamped.
  double frac(double v) const;
};

// dataMin/dataMax: value range (ignored for a bound that is fixed). A fixed bound (hasMin/hasMax)
// is used as is; otherwise the data range is padded by 5 %. A flat range gets a +-1 (or +-5 % of
// |value|) window. If fixed bounds invert (min >= max) they are ignored. `maxTicks` (>= 2) is the
// upper limit of tick marks; ticks are at 1/2/5 x 10^n.
Axis makeAxis(double dataMin, double dataMax, bool hasMin, double fixedMin, bool hasMax, double fixedMax,
              int maxTicks = 3);

}  // namespace history
