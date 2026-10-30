#include "history_graph.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace render {

namespace {

using namespace history;

void hline(M5Canvas &c, int x, int y, int w, uint32_t col) { c.fillRect(x, y, w, 1, col); }

// 2 px polyline segment.
void thickLine(M5Canvas &c, int x0, int y0, int x1, int y1, uint32_t col) {
  c.drawLine(x0, y0, x1, y1, col);
  if (std::abs(y1 - y0) > std::abs(x1 - x0)) c.drawLine(x0 + 1, y0, x1 + 1, y1, col);
  else c.drawLine(x0, y0 + 1, x1, y1 + 1, col);
}

void drawTimeAxis(M5Canvas &c, const GraphLayout &l, int64_t start, int64_t end, TzOffsetFn tz, bool gridTop) {
  const auto ticks = timeTicks(start, end, l.plot.w, tz);
  std::vector<int> cx, w;
  for (const auto &t : ticks) {
    cx.push_back(l.plot.x + t.x);
    w.push_back(textWidth(c, Font::F18, t.label));
  }
  const auto keep = fitLabels(cx, w, l.plot.x - 8, l.plot.right() + 24, 6);
  (void)gridTop;
  for (size_t i = 0; i < ticks.size(); ++i) {
    c.fillRect(cx[i] - 1, l.tickMarkY, 2, l.tickMarkH, HIST_MID);
    if (keep[i]) drawTextCy(c, Font::F18, ticks[i].label, cx[i], l.xLabelCy, Align::Center, INK);
  }
}

// Vertical grid behind the data.
void drawVGrid(M5Canvas &c, const GraphLayout &l, int64_t start, int64_t end, TzOffsetFn tz) {
  for (const auto &t : timeTicks(start, end, l.plot.w, tz)) c.fillRect(l.plot.x + t.x, l.plot.y, 1, l.plot.h, HIST_GRID);
}

void drawYAxis(M5Canvas &c, const GraphLayout &l, const Axis &a, int decimals) {
  for (int i = 0; i < 3 && i < static_cast<int>(a.ticks.size()); ++i) {
    // ticks ascend; tick 0 is the bottom one
    const int cy = l.yTickCy[2 - i];
    if (i > 0) hline(c, l.plot.x, cy, l.plot.w, HIST_GRID);
    if (i == 1 && !l.yMidLabel) continue;
    drawTextCy(c, Font::F18, formatValue(a.ticks[i], std::max(decimals, a.decimals)), l.yLabelRight, cy, Align::Right, INK);
  }
  c.fillRect(l.axisLeft.x, l.axisLeft.y, l.axisLeft.w, l.axisLeft.h, INK);
  c.fillRect(l.axisBottom.x, l.axisBottom.y, l.axisBottom.w, l.axisBottom.h, INK);
}

// Unavailable/unknown spans: no line, no band; a 4 px MID bar 8 px above the plot bottom over their width.
void drawUnavail(M5Canvas &c, const Series &s, const Rect &r) {
  const int n = std::min<int>(static_cast<int>(s.columns.size()), r.w);
  for (int i = 0; i < n; ++i)
    if (s.columns[i].unavail) c.fillRect(r.x + i, r.bottom() - 8, 1, 4, HIST_MID);
}

bool drawLine(M5Canvas &c, const HistoryGraphInput &in, const GraphLayout &l, TzOffsetFn tz) {
  const Series &s = *in.series;
  float lo, hi;
  if (!s.range(lo, hi)) return false;
  const Axis a = axis3(lo, hi, in.hasMin, in.fixedMin, in.hasMax, in.fixedMax, true);
  if (l.labels) {
    drawVGrid(c, l, s.start, s.end, tz);
    drawYAxis(c, l, a, in.decimals);
  } else {
    c.fillRect(l.axisBottom.x, l.axisBottom.y, l.axisBottom.w, l.axisBottom.h, INK);
  }
  const int n = std::min<int>(static_cast<int>(s.columns.size()), l.plot.w);
  // band, then the polyline
  for (int i = 0; i < n; ++i) {
    const Column &k = s.columns[i];
    if (!k.has) continue;
    const int y0 = valueY(l, a, k.max), y1 = valueY(l, a, k.min);
    c.fillRect(columnX(l, i), y0, 1, y1 - y0 + 1, HIST_GRID);
  }
  int px = 0, py = 0;
  bool havePrev = false;
  for (int i = 0; i < n; ++i) {
    const Column &k = s.columns[i];
    if (!k.has) { havePrev = false; continue; }
    const int x = columnX(l, i), y = valueY(l, a, k.last);
    if (havePrev) thickLine(c, px, py, x, y, INK);
    else c.fillRect(x, y, 2, 2, INK);
    px = x; py = y; havePrev = true;
  }
  if (havePrev) c.fillCircle(std::min(px, l.plot.right() - 1), py, 5, INK);
  drawUnavail(c, s, l.plot);
  if (l.labels) drawTimeAxis(c, l, s.start, s.end, tz, true);
  return true;
}

void drawStep(M5Canvas &c, const HistoryGraphInput &in, const GraphLayout &l, TzOffsetFn tz) {
  const Series &s = *in.series;
  const Rect &r = l.plot;
  c.fillRect(r.x, r.y, r.w, r.h, PAPER);
  if (l.labels) drawVGrid(c, l, s.start, s.end, tz);
  for (const Span &sp : laneSpans(s)) c.fillRect(r.x + sp.x0, r.y, sp.x1 - sp.x0, r.h, INK);
  // 2 px INK frame; grid ticks cross it as MID marks below
  c.fillRect(r.x, r.y, r.w, 2, INK);
  c.fillRect(r.x, r.bottom() - 2, r.w, 2, INK);
  c.fillRect(r.x, r.y, 2, r.h, INK);
  c.fillRect(r.right() - 2, r.y, 2, r.h, INK);
  drawUnavail(c, s, r);
  if (l.labels) drawTimeAxis(c, l, s.start, s.end, tz, false);
  if (l.listRows <= 0) return;  // widget lane: no changes list
  hline(c, r.x, l.hairlineY, r.w, HIST_MID);
  const auto changes = lastChanges(s, l.listRows);
  for (size_t i = 0; i < changes.size(); ++i) {
    const int cy = l.listCy0 + l.listRowH * static_cast<int>(i);
    drawTextCy(c, Font::F24, clockLabel(changes[i].epoch, tz(changes[i].epoch)), l.listX, cy, Align::Left, INK);
    drawTextCy(c, Font::F24, changes[i].on ? in.onWord : in.offWord, l.listX + l.listStateDx, cy, Align::Left, INK);
  }
}

bool drawBars(M5Canvas &c, const HistoryGraphInput &in, const GraphLayout &l, TzOffsetFn tz) {
  (void)tz;
  const int n = static_cast<int>(in.bars.size());
  if (n == 0) return false;
  double mx = 0;
  bool any = false;
  for (const auto &b : in.bars)
    if (b.has) { mx = any ? std::max<double>(mx, b.value) : b.value; any = true; }
  if (!any) return false;
  const bool labels = barLabels(n) && l.labels && (l.plot.w / n >= l.minBarLabelSlot);
  const Axis a = barAxis(mx, in.hasMin, in.fixedMin, in.hasMax, in.fixedMax, l.plot.h, labels);
  const auto slots = barSlots(l.plot, n);
  if (l.labels) drawYAxis(c, l, a, in.decimals);
  else c.fillRect(l.axisBottom.x, l.axisBottom.y, l.axisBottom.w, l.axisBottom.h, INK);
  std::vector<int> cx, w;
  for (int i = 0; i < n; ++i) {
    const auto &b = in.bars[i];
    const auto &sl = slots[i];
    if (b.has) {
      const int top = std::min(valueY(l, a, b.value), l.plot.bottom() - 1);
      const int h = l.plot.bottom() - top;
      if (b.partial) {
        c.fillRect(sl.barX, top, sl.barW, h, INK);
        if (sl.barW > 4 && h > 4) c.fillRect(sl.barX + 2, top + 2, sl.barW - 4, h - 4, HIST_MID);
      } else {
        c.fillRect(sl.barX, top, sl.barW, h, INK);
      }
      if (labels) drawTextCy(c, Font::F18, formatValue(b.value, in.decimals), sl.barX + sl.barW / 2, top - 14, Align::Center, INK);
    }
    cx.push_back(sl.slotX + sl.slotW / 2);
    w.push_back(textWidth(c, Font::F18, b.label));
  }
  const auto keep = fitLabels(cx, w, l.plot.x - 8, l.plot.right() + 24, 4);
  for (int i = 0; i < n && l.labels; ++i)
    if (keep[i]) drawTextCy(c, Font::F18, in.bars[i].label, cx[i], l.xLabelCy, Align::Center, INK);
  return true;
}

}  // namespace

int localUtcOffset(int64_t epoch) {
  const time_t t = static_cast<time_t>(epoch);
  struct tm lt;
  localtime_r(&t, &lt);
  const int64_t local = history::daysFromCivil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday) * 86400 +
                        lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
  return static_cast<int>(local - epoch);
}

bool drawHistoryGraph(M5Canvas &c, const HistoryGraphInput &in) {
  const GraphLayout l = in.layout ? *in.layout : graphLayout(in.type, in.landscape);
  const TzOffsetFn tz = in.tz ? in.tz : localUtcOffset;
  if (in.type == ChartType::Bar) return drawBars(c, in, l, tz);
  if (!in.series || in.series->columns.empty() || in.series->empty()) return false;
  if (in.type == ChartType::Step) {
    drawStep(c, in, l, tz);
    return true;
  }
  return drawLine(c, in, l, tz);
}

}  // namespace render
