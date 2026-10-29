#include "axis.h"

#include <algorithm>
#include <cmath>

namespace history {

double Axis::frac(double v) const {
  if (hi <= lo) return 0;
  return std::min(1.0, std::max(0.0, (v - lo) / (hi - lo)));
}

static double niceStep(double raw) {
  const double e = std::pow(10.0, std::floor(std::log10(raw)));
  const double f = raw / e;
  const double n = f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10;
  return n * e;
}

Axis makeAxis(double dMin, double dMax, bool hasMin, double fMin, bool hasMax, double fMax, int maxTicks) {
  if (hasMin && hasMax && fMin >= fMax) hasMin = hasMax = false;
  maxTicks = std::max(2, maxTicks);
  if (dMin > dMax) std::swap(dMin, dMax);
  double lo = dMin, hi = dMax;
  if (hi - lo < 1e-9) {
    const double w = std::max(1.0, std::fabs(lo) * 0.05);
    lo -= w;
    hi += w;
  } else {
    const double pad = (hi - lo) * 0.05;
    lo -= pad;
    hi += pad;
  }
  if (hasMin) lo = fMin;
  if (hasMax) hi = fMax;
  if (hi <= lo) {  // one fixed bound ran past the data
    if (hasMin) hi = lo + 1; else lo = hi - 1;
  }
  Axis a;
  a.lo = lo;
  a.hi = hi;
  // Smallest nice step that gives at most maxTicks ticks.
  double step = niceStep((hi - lo) / (maxTicks - 1));
  auto count = [&](double s) { return static_cast<int>(std::floor(hi / s + 1e-9) - std::ceil(lo / s - 1e-9)) + 1; };
  while (count(step) > maxTicks) step = niceStep(step * 1.01 * 2);
  a.step = step;
  a.decimals = 0;
  while (a.decimals < 6 && std::fabs(step * std::pow(10, a.decimals) - std::round(step * std::pow(10, a.decimals))) > 1e-6)
    ++a.decimals;
  for (double t = std::ceil(lo / step - 1e-9); t * step <= hi + step * 1e-9; ++t) {
    double v = t * step;
    if (std::fabs(v) < step * 1e-9) v = 0;
    a.ticks.push_back(v);
  }
  return a;
}

}  // namespace history
