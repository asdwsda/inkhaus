#include "history_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace history {

GraphLayout graphLayout(ChartType type, bool landscape) {
  GraphLayout l;
  l.type = type;
  l.landscape = landscape;
  if (type == ChartType::Step) {
    l.plot = landscape ? Rect{56, 186, 824, 96} : Rect{32, 372, 452, 96};
    l.xLabelCy = l.plot.bottom() + 22;
    l.tickMarkY = l.plot.bottom() + 1;
    l.tickMarkH = 7;
    l.hairlineY = l.plot.bottom() + 40;
    l.listCy0 = l.hairlineY + 20;
    l.listX = l.plot.x;
  } else {
    l.plot = landscape ? Rect{116, 186, 764, 240} : Rect{92, 372, 392, 264};
    l.axisLeft = Rect{l.plot.x - 1, l.plot.y - 4, 2, l.plot.h + 5};
    l.axisBottom = Rect{l.plot.x - 1, l.plot.bottom(), l.plot.w + 2, 2};
    l.yLabelRight = l.plot.x - 8;
    l.yTickCy[0] = l.plot.y;
    l.yTickCy[1] = l.plot.y + l.plot.h / 2;
    l.yTickCy[2] = l.plot.bottom();
    l.xLabelCy = l.plot.bottom() + 22;
    l.tickMarkY = l.plot.bottom() + 2;
    l.tickMarkH = 6;
  }
  l.maxXTicks = l.plot.w / 90;
  return l;
}

int timeToX(int64_t t, int64_t start, int64_t end, int w) {
  if (end <= start || w <= 0) return 0;
  if (t <= start) return 0;
  if (t >= end) return w;
  return static_cast<int>((t - start) * w / (end - start));
}

int valueY(const GraphLayout& l, const Axis& a, double v) {
  return l.plot.bottom() - static_cast<int>(std::lround(a.frac(v) * l.plot.h));
}

namespace {

// 1 / 2 / 2.5 / 5 x 10^n ladder of half-axis steps, ascending from 10^nMin.
double ladder(int idx, int nMin) {
  static const double m[4] = {1, 2, 2.5, 5};
  return m[idx % 4] * std::pow(10.0, nMin + idx / 4);
}

int decimalsOf(double step) {
  int d = 0;
  while (d < 6 && std::fabs(step * std::pow(10, d) - std::round(step * std::pow(10, d))) > 1e-6) ++d;
  return d;
}

Axis finish3(double lo, double hi) {
  Axis a;
  a.lo = lo;
  a.hi = hi;
  a.step = (hi - lo) / 2;
  a.decimals = decimalsOf(a.step);
  a.ticks = {lo, lo + a.step, hi};
  return a;
}

}  // namespace

Axis axis3(double dMin, double dMax, bool hasMin, double fMin, bool hasMax, double fMax, bool pad) {
  if (hasMin && hasMax && fMin >= fMax) hasMin = hasMax = false;
  if (dMin > dMax) std::swap(dMin, dMax);
  double lo = dMin, hi = dMax;
  if (hi - lo < 1e-9) {
    const double w = std::max(1.0, std::fabs(lo) * 0.05);
    lo -= w;
    hi += w;
  } else if (pad) {
    const double p = (hi - lo) * 0.05;
    lo -= p;
    hi += p;
  }
  if (hasMin) lo = fMin;
  if (hasMax) hi = fMax;
  if (hi <= lo) {
    if (hasMin) hi = lo + 1; else lo = hi - 1;
  }
  if (hasMin && hasMax) return finish3(lo, hi);
  const double half = (hi - lo) / 2;
  const int nMin = static_cast<int>(std::floor(std::log10(half))) - 1;
  for (int i = 0; i < 40; ++i) {
    const double h = ladder(i, nMin);
    const double g = std::pow(10.0, std::floor(std::log10(h) + 1e-9));  // grid of the lowest tick
    if (hasMax) {
      double l2 = std::floor((hi - 2 * h) / g + 1e-9) * g;  // hi fixed: go down
      if (l2 <= lo + 1e-9) return finish3(std::min(l2, lo), hi);
    } else {
      const double l2 = hasMin ? lo : std::floor(lo / g + 1e-9) * g;
      if (l2 + 2 * h >= hi - 1e-9) return finish3(l2, l2 + 2 * h);
    }
  }
  return finish3(lo, hi);
}

Axis barAxis(double maxValue, bool hasMin, double fMin, bool hasMax, double fMax, int plotH, bool labels) {
  const double lo = hasMin ? fMin : 0;
  if (hasMax && fMax > lo) return finish3(lo, fMax);
  double top = std::max(maxValue, lo + 1e-6);
  const double room = labels ? 25.0 : 0.0;
  if (labels && plotH > room + 10) top = lo + (top - lo) * plotH / (plotH - room);
  return axis3(lo, top, true, lo, false, 0, false);
}

std::string formatValue(double v, int decimals) {
  char b[40];
  snprintf(b, sizeof b, "%.*f", std::max(0, std::min(decimals, 6)), v);
  std::string s = b;
  if (s.size() > 1 && s[0] == '-') {
    bool zero = true;
    for (char c : s)
      if (c >= '1' && c <= '9') zero = false;
    if (zero) s.erase(0, 1);
  }
  return s;
}

// ---- time axis ---------------------------------------------------------------------------------

int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

namespace {

int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

void civilFromDays(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = floorDiv(z, 146097);
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  y = static_cast<int>(yoe + era * 400 + (m <= 2));
}

const int64_t kSteps[] = {3600, 7200, 10800, 21600, 43200, 86400, 172800, 604800};

std::vector<TimeTick> ticksFor(int64_t step, int64_t start, int64_t end, int w, TzOffsetFn tz) {
  std::vector<TimeTick> out;
  const int64_t anchor = step == 604800 ? 4 * 86400 : 0;  // Mondays
  const int64_t ls = start + tz(start);
  int64_t k = floorDiv(ls - anchor, step) + 1;
  for (;; ++k) {
    const int64_t local = anchor + k * step;
    const int64_t t = local - tz(local - tz(start));
    if (t >= end) break;
    if (t <= start) continue;
    const int x = timeToX(t, start, end, w);
    if (x < 1 || x > w - 1) continue;
    TimeTick tk;
    tk.epoch = t;
    tk.x = x;
    char b[16];
    if (step < 86400) {
      snprintf(b, sizeof b, "%02d:00", static_cast<int>(floorDiv(local, 3600) % 24 + 24) % 24);
    } else if (end - start > 8 * 86400) {
      int y, m, d;
      civilFromDays(floorDiv(local, 86400), y, m, d);
      snprintf(b, sizeof b, "%d.%d.", d, m);
    } else {
      static const char* wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
      snprintf(b, sizeof b, "%s", wd[(floorDiv(local, 86400) % 7 + 7 + 4) % 7]);
    }
    tk.label = b;
    out.push_back(tk);
    if (out.size() > 400) break;
  }
  return out;
}

}  // namespace

int64_t timeTickStep(int64_t start, int64_t end, int plotW, TzOffsetFn tz) {
  if (end <= start || plotW <= 0) return 0;
  const size_t maxT = static_cast<size_t>(std::max(1, plotW / 90));
  for (int64_t s : kSteps)
    if (ticksFor(s, start, end, plotW, tz).size() <= maxT) return s;
  return kSteps[sizeof kSteps / sizeof kSteps[0] - 1];
}

std::vector<TimeTick> timeTicks(int64_t start, int64_t end, int plotW, TzOffsetFn tz) {
  const int64_t s = timeTickStep(start, end, plotW, tz);
  if (s == 0) return {};
  return ticksFor(s, start, end, plotW, tz);
}

std::vector<bool> fitLabels(const std::vector<int>& cx, const std::vector<int>& w, int minX, int maxX, int gap) {
  std::vector<bool> keep(cx.size(), false);
  int lastRight = minX - gap - 1;
  for (size_t i = 0; i < cx.size(); ++i) {
    const int l = cx[i] - w[i] / 2, r = l + w[i];
    if (l < minX || r > maxX || l < lastRight + gap) continue;
    keep[i] = true;
    lastRight = r;
  }
  return keep;
}

std::string clockLabel(int64_t epoch, int tzOffset) {
  const int64_t l = epoch + tzOffset;
  const int sod = static_cast<int>(((l % 86400) + 86400) % 86400);
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", sod / 3600, sod / 60 % 60);
  return b;
}

// ---- step lane ---------------------------------------------------------------------------------

std::vector<Span> laneSpans(const Series& s, int minWidth) {
  std::vector<Span> out;
  const int n = static_cast<int>(s.columns.size());
  int i = 0;
  while (i < n) {
    if (!(s.columns[i].has && s.columns[i].max >= 0.5f)) { ++i; continue; }
    int j = i;
    while (j < n && s.columns[j].has && s.columns[j].max >= 0.5f) ++j;
    Span sp{i, j};
    if (sp.x1 - sp.x0 < minWidth) {
      const int grow = minWidth - (sp.x1 - sp.x0);
      sp.x0 -= grow / 2;
      sp.x1 = sp.x0 + minWidth;
      if (sp.x0 < 0) { sp.x0 = 0; sp.x1 = std::min(n, minWidth); }
      if (sp.x1 > n) { sp.x1 = n; sp.x0 = std::max(0, n - minWidth); }
    }
    if (!out.empty() && sp.x0 <= out.back().x1) out.back().x1 = std::max(out.back().x1, sp.x1);
    else out.push_back(sp);
    i = j;
  }
  return out;
}

std::vector<Change> lastChanges(const Series& s, int maxRows) {
  std::vector<Change> all;
  const int n = static_cast<int>(s.columns.size());
  if (n == 0 || s.end <= s.start) return all;
  bool prevKnown = false, prev = false;
  for (int i = 0; i < n; ++i) {
    const Column& c = s.columns[i];
    if (!c.has) continue;
    const bool last = c.last >= 0.5f;
    const int64_t t = s.start + static_cast<int64_t>(i) * (s.end - s.start) / n;
    if (prevKnown) {
      const bool mixed = c.min < 0.5f && c.max >= 0.5f;
      if (mixed && last == prev) {  // blip: left the state and came back within the column
        all.push_back({t, !prev});
        all.push_back({t, prev});
      } else if (last != prev) {
        all.push_back({t, last});
      }
    }
    prev = last;
    prevKnown = true;
  }
  std::vector<Change> out;
  for (size_t i = all.size(); i > 0 && static_cast<int>(out.size()) < maxRows; --i) out.push_back(all[i - 1]);
  return out;
}

StepSummary stepSummary(const Series& s) {
  StepSummary r;
  const int n = static_cast<int>(s.columns.size());
  if (n == 0 || s.end <= s.start) return r;
  bool inOn = false;
  int onCols = 0;
  for (int i = 0; i < n; ++i) {
    const Column& c = s.columns[i];
    const bool on = c.has && c.max >= 0.5f;
    if (on) {
      ++onCols;
      if (!inOn) ++r.episodes;
    }
    inOn = c.has && c.last >= 0.5f;
  }
  r.onSeconds = static_cast<int64_t>(onCols) * (s.end - s.start) / n;
  return r;
}

// ---- bars --------------------------------------------------------------------------------------

std::vector<BarSlot> barSlots(const Rect& plot, int n) {
  std::vector<BarSlot> out;
  if (n <= 0) return out;
  const int slot = plot.w / n;
  const int bw = std::max(1, slot >= 30 ? slot - 20 : slot - 4);
  for (int i = 0; i < n; ++i) {
    BarSlot b;
    b.slotX = plot.x + i * plot.w / n;
    b.slotW = slot;
    b.barW = std::min(bw, std::max(1, slot));
    b.barX = b.slotX + (slot - b.barW) / 2;
    out.push_back(b);
  }
  return out;
}

}  // namespace history
