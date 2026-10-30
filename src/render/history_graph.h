#pragma once
// History graphs drawn into the popup's canvas (docs/ui/history.md section 3, task 052). Draws the
// plot area only (axes, grid, data, tick labels, last-changes list); the panel, title row, range
// lines, legend and note belong to the popup (task 053). Layout maths: lib/history/history_layout.h.
#include <string>
#include <vector>

#include "draw.h"
#include "history_layout.h"

namespace render {

// The two chart-only greys (docs/ui/history.md section 2), 24-bit.
constexpr uint32_t HIST_GRID = 0xD0D0D0U;
constexpr uint32_t HIST_MID = 0x808080U;

struct HistoryBar {
  float value = 0;
  bool has = false;
  bool partial = false;  // newest, incomplete bucket: MID fill + INK outline
  std::string label;     // x label under the bar ("Fri", "Today", "14")
};

struct HistoryGraphInput {
  history::ChartType type = history::ChartType::Line;
  bool landscape = false;
  const history::Series *series = nullptr;  // Line / Step (columns == layout.columns())
  std::vector<HistoryBar> bars;             // Bar
  int decimals = 1;                         // widget decimals (y labels, bar labels)
  bool hasMin = false, hasMax = false;
  double fixedMin = 0, fixedMax = 0;
  std::string onWord = "Open", offWord = "Closed";  // step list state words
  history::TzOffsetFn tz = nullptr;                 // null: UTC offset from the C library TZ
  const history::GraphLayout *layout = nullptr;     // widget graphs (task 071): absolute layout; null = the popup's
};

// Local UTC offset in seconds at `epoch` from the C library's TZ.
int localUtcOffset(int64_t epoch);

// Draws into `c`. Does not refresh. Returns false if there is nothing to draw (no series/bars).
bool drawHistoryGraph(M5Canvas &c, const HistoryGraphInput &in);

}  // namespace render
