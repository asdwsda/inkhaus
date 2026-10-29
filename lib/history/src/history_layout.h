#pragma once
// Pure layout maths of the history graphs (docs/ui/history.md sections 3 and 4). No drawing, no
// hardware: the renderer in src/render/history_graph.cpp only places what is computed here.
#include <cstdint>
#include <string>
#include <vector>

#include "axis.h"
#include "collector.h"

namespace history {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  int right() const { return x + w; }
  int bottom() const { return y + h; }
};

enum class ChartType : uint8_t { Line, Bar, Step };

// Fixed geometry per orientation and chart type.
struct GraphLayout {
  ChartType type = ChartType::Line;
  bool landscape = false;
  Rect plot;                 // line / bar: the plot rect; step: the lane
  Rect axisLeft, axisBottom; // 2 px INK axes (line / bar only; zero-size for step)
  int yLabelRight = 0;       // right edge of the y tick labels
  int yTickCy[3] = {0, 0, 0};// top, middle, bottom (line / bar only)
  int xLabelCy = 0;          // centre line of the time / bucket labels
  int tickMarkY = 0, tickMarkH = 0;  // x tick marks (2 px wide, MID)
  int maxXTicks = 0;         // floor(width / 90)
  int hairlineY = 0;         // step: separator under the lane
  int listCy0 = 0, listRowH = 36, listRows = 4;  // step: last changes list
  int listX = 0, listStateDx = 100;
  int columns() const { return plot.w; }  // columns a Collector must produce
};

GraphLayout graphLayout(ChartType type, bool landscape);

// x of column i (left edge) and the column a time falls in.
inline int columnX(const GraphLayout& l, int i) { return l.plot.x + i; }
int timeToX(int64_t t, int64_t start, int64_t end, int w);  // 0..w, relative to the plot's left edge
// y for a value on an axis: plot bottom for axis.lo, plot top for axis.hi (clamped).
int valueY(const GraphLayout& l, const Axis& a, double v);

// 3-tick y axis (min / middle / max) rounded outward to values with short labels. pad: pad the data
// range by 5 % first (line chart). A fixed bound is used as is. Result ticks are always 3 values.
Axis axis3(double dataMin, double dataMax, bool hasMin, double fixedMin, bool hasMax, double fixedMax, bool pad);
// Bar axis: starts at 0 (or the fixed min); top rounded up so the tallest bar plus a value label
// (25 px above the bar) fits into plotH when `labels`.
Axis barAxis(double maxValue, bool hasMin, double fixedMin, bool hasMax, double fixedMax, int plotH, bool labels);
// Label for a tick/value: `decimals` digits (at least axis.decimals), no "-0".
std::string formatValue(double v, int decimals);

// ---- time axis -----------------------------------------------------------------------------
struct TimeTick {
  int64_t epoch = 0;
  int x = 0;           // relative to the plot's left edge, 1..w-1
  std::string label;
};
// `tz` returns the UTC offset in seconds that applies at an epoch (local = epoch + offset).
using TzOffsetFn = int (*)(int64_t epoch);
std::vector<TimeTick> timeTicks(int64_t start, int64_t end, int plotW, TzOffsetFn tz);
// Chosen step in seconds (0 if no window).
int64_t timeTickStep(int64_t start, int64_t end, int plotW, TzOffsetFn tz);

// Keeps labels (centre cx, width w) inside [minX, maxX] and at least `gap` px apart; earlier
// labels win. Returns one flag per label.
std::vector<bool> fitLabels(const std::vector<int>& cx, const std::vector<int>& w, int minX, int maxX, int gap);

// Calendar helpers (proleptic Gregorian, days since 1970-01-01).
int64_t daysFromCivil(int y, int m, int d);
std::string clockLabel(int64_t epoch, int tzOffset);  // "HH:MM" local

// ---- step lane -----------------------------------------------------------------------------
struct Span { int x0 = 0, x1 = 0; };  // [x0, x1) in columns relative to the lane's left edge
// Columns in which the entity was on (Column.max >= 0.5), merged; every span at least 3 wide.
std::vector<Span> laneSpans(const Series& s, int minWidth = 3);

struct Change { int64_t epoch = 0; bool on = false; };
// Newest first, at most maxRows. Time = start of the column the change falls in. A column that
// went back to its previous state (min != max) counts as two changes.
std::vector<Change> lastChanges(const Series& s, int maxRows = 4);
struct StepSummary { int episodes = 0; int64_t onSeconds = 0; };  // "4x open, 38 min total"
StepSummary stepSummary(const Series& s);

// ---- bars ----------------------------------------------------------------------------------
struct BarSlot {
  int slotX = 0, slotW = 0;  // slot rect (x absolute)
  int barX = 0, barW = 0;
};
// N slots share the plot width; bar width = slot - 20 (slot - 4 when slots are narrow, min 1).
std::vector<BarSlot> barSlots(const Rect& plot, int n);
inline bool barLabels(int n) { return n >= 1 && n <= 8; }

}  // namespace history
