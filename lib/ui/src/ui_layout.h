#pragma once
// Pure layout engine (docs/ui/layout.md). No drawing, no Arduino includes.
#include <cstdint>
#include <vector>

#include "config_model.h"
#include "settings.h"

namespace ui {

struct Point {
  int x = 0, y = 0;
};

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  bool contains(Point p) const { return contains(p.x, p.y); }
  bool operator==(const Rect &o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
  bool operator!=(const Rect &o) const { return !(*this == o); }
};

bool rectsOverlap(const Rect &a, const Rect &b);
bool rectInside(const Rect &inner, const Rect &outer);
Rect inflate(const Rect &r, int by);

constexpr int kGap = 12;
constexpr int kCellH = 104;
constexpr int kMaxCols = 5;
constexpr int kMaxRows = 7;
constexpr int kMaxTabSlots = 5;

// Fixed geometry of one orientation (layout.md section 2).
struct Grid {
  settings::Orientation orientation;
  int screenW, screenH;
  int cols, rows;
  int cellW;
  int colX[kMaxCols];
  int rowY[kMaxRows];
  Rect statusBar, tabBar, content;
  Rect refreshHit, settingsHit, sleepBadge;
  Rect slotRect[kMaxTabSlots];  // first tabSlots entries are valid
  int tabSlots;
};
// `columns` (4 or 5) only matters in landscape; anything else gives the 5-column grid.
const Grid &gridFor(settings::Orientation o, int columns = 5);

// Pixel rect of a w x h cell block with top-left cell (c, r).
Rect cellRect(const Grid &g, int c, int r, int w, int h);

// ---- widget sizes (section 3) ----
struct SizeLimits {
  config::WidgetSize def, min, max;
};
// Per grid (layout.md section 3): wide types are 2 cells in portrait and landscape 4 columns, 3 in
// landscape 5 columns.
SizeLimits sizeLimits(config::WidgetType type, const Grid &g);
SizeLimits sizeLimits(config::WidgetType type, settings::Orientation o, int columns = 5);

enum ClampReason : unsigned {
  kClampTypeMin = 1,   // raised to the type minimum
  kClampTypeMax = 2,   // lowered to the type maximum
  kClampColumns = 4,   // wider than the grid
  kClampRows = 8,      // taller than rows-1 (safeguard; needs a synthetic type table to trigger)
};

struct ClampReport {
  int widgetIndex = 0;
  config::WidgetSize requested;
  config::WidgetSize used;
  unsigned reasons = 0;  // ClampReason bits
};

// The clamp rule (section 3) with an explicit limits table. `req` is the requested or default size.
config::WidgetSize clampSize(const SizeLimits &lim, config::WidgetSize req, const Grid &g,
                             unsigned *reasons = nullptr);
// Applies the clamp rule to the requested (or default) size. reasons is 0 if unchanged.
config::WidgetSize resolveSize(config::WidgetType type, const config::Widget &w,
                               const Grid &g, unsigned *reasons = nullptr,
                               config::WidgetSize *requested = nullptr);

// ---- flow (section 4) ----
struct Placement {
  int index = 0;  // index in the tab's widget list
  config::WidgetType type = config::WidgetType::Sensor;
  int page = 0;
  Rect rect;
  int cellX = 0, cellY = 0, cellW = 1, cellH = 1;
  bool clamped = false;
  bool history = false;  // sensor/binary_sensor with history enabled: rocker-focusable (opens the popup)
};

struct Flow {
  std::vector<Placement> placements;  // all pages, config order
  std::vector<ClampReport> clampReports;
  int pageCount = 1;
  bool pagerVisible = false;
};

Flow computeFlow(settings::Orientation o, const std::vector<config::Widget> &widgets,
                 int columns = 5);

// ---- tab bar groups (section 5) ----
int tabsPerGroup(settings::Orientation o, int tabCount);  // slots if no overflow, else slots-1
int groupCount(settings::Orientation o, int tabCount);    // 1 if no overflow
int groupForTab(settings::Orientation o, int tabCount, int tab);
int nextGroup(settings::Orientation o, int tabCount, int group);

enum class SlotKind { Tab, Overflow, Empty };
struct TabSlot {
  Rect rect;
  SlotKind kind = SlotKind::Empty;
  int tabIndex = -1;
};

struct Pager {
  Rect rect, prev, next, label;
  bool visible = false;
  int subPage = 0;
  int subPageCount = 1;
};

struct Layout {
  settings::Orientation orientation = settings::Orientation::Portrait;
  Rect statusBar, tabBar, content;
  Rect refreshHit, settingsHit;  // the two touchable status areas
  Rect sleepBadge;           // drawn by the firmware only while asleep; not a hit target
  std::vector<TabSlot> tabSlots;
  int activeTab = 0;
  int group = 0, groupCount = 1;
  Pager pager;
  std::vector<Placement> widgets;  // active sub-page only
  Flow flow;                       // all sub-pages
};

// activeTab/subPage/group are clamped into range. group < 0 means "derive from activeTab".
Layout computeLayout(settings::Orientation o, const std::vector<config::Tab> &tabs,
                     int activeTab, int subPage, int group = -1);

// Sleep badge region of an orientation (STYLE.md 5.5), the one constant for drawing/clearing it on its own.
Rect sleepBadgeRect(settings::Orientation o);

// ---- sub-regions (widgets.md summary) ----
struct SubRegion {
  int id = 0;
  Rect rect;
};
// rect is the widget's pixel rect; result rects are absolute. All buttons are 64 x 64 at
// y = 20: cover/vacuum at W-228, W-152, W-76; climate minus W-276, plus W-76. `size` is
// unused (every type has one row layout) and kept for call-site stability.
std::vector<SubRegion> subRegions(config::WidgetType type, config::WidgetSize size,
                                  const Rect &rect);

// ---- hit testing (section 6) ----
enum class HitKind { None, Tab, TabOverflow, Refresh, Settings, PagerPrev, PagerNext, Widget };
struct Hit {
  HitKind kind = HitKind::None;
  int index = 0;
  int sub = 0;
  bool operator==(const Hit &o) const { return kind == o.kind && index == o.index && sub == o.sub; }
};
Hit hitTest(const Layout &layout, Point p);

}  // namespace ui
