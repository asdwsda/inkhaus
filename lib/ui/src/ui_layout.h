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

constexpr int kGap = 16;
constexpr int kCellH = 144;
constexpr int kMaxCols = 4;
constexpr int kMaxRows = 5;

// Fixed geometry of one orientation (layout.md section 2).
struct Grid {
  int screenW, screenH;
  int cols, rows;
  int cellW;
  int colX[kMaxCols];
  int rowY[kMaxRows];
  Rect statusBar, tabBar, content;
  int tabSlots;
};
const Grid &gridFor(settings::Orientation o);

// Pixel rect of a w x h cell block with top-left cell (c, r).
Rect cellRect(const Grid &g, int c, int r, int w, int h);

// ---- widget sizes (section 3) ----
struct SizeLimits {
  config::WidgetSize def, min, max;
};
SizeLimits sizeLimits(config::WidgetType type);

enum ClampReason : unsigned {
  kClampTypeMin = 1,   // raised to the type minimum
  kClampTypeMax = 2,   // lowered to the type maximum
  kClampColumns = 4,   // wider than the grid
  kClampRows = 8,      // taller than rows-1
};

struct ClampReport {
  int widgetIndex = 0;
  config::WidgetSize requested;
  config::WidgetSize used;
  unsigned reasons = 0;  // ClampReason bits
};

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
};

struct Flow {
  std::vector<Placement> placements;  // all pages, config order
  std::vector<ClampReport> clampReports;
  int pageCount = 1;
  bool pagerVisible = false;
};

Flow computeFlow(settings::Orientation o, const std::vector<config::Widget> &widgets);

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

// ---- sub-regions (widgets.md summary) ----
struct SubRegion {
  int id = 0;
  Rect rect;
};
// rect is the widget's pixel rect; result rects are absolute.
std::vector<SubRegion> subRegions(config::WidgetType type, config::WidgetSize size,
                                  const Rect &rect, bool climateModeEnabled = false);

// ---- hit testing (section 6) ----
enum class HitKind { None, Tab, TabOverflow, PagerPrev, PagerNext, Widget };
struct Hit {
  HitKind kind = HitKind::None;
  int index = 0;
  int sub = 0;
  bool operator==(const Hit &o) const { return kind == o.kind && index == o.index && sub == o.sub; }
};
Hit hitTest(const Layout &layout, Point p, bool climateModeEnabled = false);

}  // namespace ui
