#include "ui_layout.h"

#include <algorithm>

namespace ui {

using config::WidgetSize;
using config::WidgetType;
using settings::Orientation;

bool rectsOverlap(const Rect &a, const Rect &b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
bool rectInside(const Rect &i, const Rect &o) {
  return i.x >= o.x && i.y >= o.y && i.x + i.w <= o.x + o.w && i.y + i.h <= o.y + o.h;
}
Rect inflate(const Rect &r, int by) { return {r.x - by, r.y - by, r.w + 2 * by, r.h + 2 * by}; }

const Grid &gridFor(Orientation o, int columns) {
  static const Grid portrait = {
      Orientation::Portrait, 540, 960, 2, 7, 248,
      {16, 276, 0, 0, 0},
      {68, 184, 300, 416, 532, 648, 764},
      {0, 0, 540, 56}, {0, 880, 540, 80}, {16, 68, 508, 800},
      {0, 0, 288, 56}, {412, 0, 112, 56}, {298, 3, 100, 48},
      {{16, 892, 118, 56}, {146, 892, 118, 56}, {276, 892, 118, 56}, {406, 892, 118, 56}, {}},
      4};
  static const Grid landscape = {
      Orientation::Landscape, 960, 540, 5, 4, 176,
      {16, 204, 392, 580, 768},
      {72, 188, 304, 420, 0, 0, 0},
      {0, 0, 960, 64}, {172, 0, 648, 64}, {16, 72, 928, 452},
      {16, 4, 144, 56}, {832, 4, 112, 56}, {710, 8, 100, 48},
      {{172, 4, 120, 56}, {304, 4, 120, 56}, {436, 4, 120, 56}, {568, 4, 120, 56}, {700, 4, 120, 56}},
      5};
  static const Grid landscape4 = {
      Orientation::Landscape, 960, 540, 4, 4, 223,
      {16, 251, 486, 721, 0},
      {72, 188, 304, 420, 0, 0, 0},
      {0, 0, 960, 64}, {172, 0, 648, 64}, {16, 72, 928, 452},
      {16, 4, 144, 56}, {832, 4, 112, 56}, {710, 8, 100, 48},
      {{172, 4, 120, 56}, {304, 4, 120, 56}, {436, 4, 120, 56}, {568, 4, 120, 56}, {700, 4, 120, 56}},
      5};
  if (o == Orientation::Portrait) return portrait;
  return columns == 4 ? landscape4 : landscape;
}

Rect sleepBadgeRect(Orientation o) { return gridFor(o).sleepBadge; }

Rect cellRect(const Grid &g, int c, int r, int w, int h) {
  return {g.colX[c], g.rowY[r], w * g.cellW + (w - 1) * kGap, h * kCellH + (h - 1) * kGap};
}

SizeLimits sizeLimits(WidgetType t, Orientation o, int columns) {
  return sizeLimits(t, gridFor(o, columns));
}

SizeLimits sizeLimits(WidgetType t, const Grid &g) {
  const int wide = g.cols == 5 ? 3 : 2;
  switch (t) {
    case WidgetType::Cover:
    case WidgetType::Climate:
    case WidgetType::Vacuum: return {{wide, 1}, {wide, 1}, {wide, 1}};
    case WidgetType::Light:
    case WidgetType::Switch:
    case WidgetType::Automation:
    case WidgetType::BinarySensor: return {{1, 1}, {1, 1}, {1, 1}};
    case WidgetType::Sensor: return {{1, 1}, {1, 1}, {2, 1}};
    case WidgetType::Weather: return {{2, 1}, {1, 1}, {2, 1}};
  }
  return {{1, 1}, {1, 1}, {1, 1}};
}

WidgetSize clampSize(const SizeLimits &lim, WidgetSize req, const Grid &g, unsigned *reasons) {
  WidgetSize s = req;
  unsigned why = 0;
  if (s.w < lim.min.w) { s.w = lim.min.w; why |= kClampTypeMin; }
  if (s.h < lim.min.h) { s.h = lim.min.h; why |= kClampTypeMin; }
  if (s.w > lim.max.w) { s.w = lim.max.w; why |= kClampTypeMax; }
  if (s.h > lim.max.h) { s.h = lim.max.h; why |= kClampTypeMax; }
  if (s.w > g.cols) { s.w = g.cols; why |= kClampColumns; }
  if (s.h > g.rows - 1) { s.h = g.rows - 1; why |= kClampRows; }
  if (s.w < 1) s.w = 1;  // only reachable with a hostile limits table
  if (s.h < 1) s.h = 1;
  if (reasons) *reasons = why;
  return s;
}

WidgetSize resolveSize(WidgetType type, const config::Widget &w, const Grid &g,
                       unsigned *reasons, WidgetSize *requested) {
  SizeLimits lim = sizeLimits(type, g);
  if (config::isGraphWidget(w)) lim = {{2, 2}, {2, 1}, {g.cols, config::kMaxWidgetSize}};  // task 071: wide graph, default 2x2
  WidgetSize req = w.size ? *w.size : lim.def;
  if (requested) *requested = req;
  return clampSize(lim, req, g, reasons);
}

namespace {

struct Occ {
  bool cell[kMaxRows][kMaxCols] = {};
};

bool firstFit(const Occ &o, int cols, int rows, int w, int h, int &oc, int &orow) {
  for (int r = 0; r + h <= rows; ++r) {
    for (int c = 0; c + w <= cols; ++c) {
      bool free = true;
      for (int dy = 0; dy < h && free; ++dy)
        for (int dx = 0; dx < w; ++dx)
          if (o.cell[r + dy][c + dx]) { free = false; break; }
      if (free) { oc = c; orow = r; return true; }
    }
  }
  return false;
}

struct Slot { int w, h, page, c, r; };

int place(const std::vector<WidgetSize> &sizes, int cols, int rows, std::vector<Slot> &out) {
  std::vector<Occ> pages(1);
  out.assign(sizes.size(), Slot{});
  for (size_t i = 0; i < sizes.size(); ++i) {
    int c = 0, r = 0;
    if (!firstFit(pages.back(), cols, rows, sizes[i].w, sizes[i].h, c, r)) {
      pages.emplace_back();
      if (!firstFit(pages.back(), cols, rows, sizes[i].w, sizes[i].h, c, r)) { c = 0; r = 0; }
    }
    for (int dy = 0; dy < sizes[i].h; ++dy)
      for (int dx = 0; dx < sizes[i].w; ++dx) pages.back().cell[r + dy][c + dx] = true;
    out[i] = {sizes[i].w, sizes[i].h, (int)pages.size() - 1, c, r};
  }
  return (int)pages.size();
}

}  // namespace

Flow computeFlow(Orientation o, const std::vector<config::Widget> &widgets, int columns) {
  const Grid &g = gridFor(o, columns);
  Flow f;
  std::vector<WidgetSize> sizes;
  std::vector<unsigned> reasons;
  for (size_t i = 0; i < widgets.size(); ++i) {
    unsigned why = 0;
    WidgetSize req;
    WidgetSize s = resolveSize(widgets[i].type, widgets[i], g, &why, &req);
    sizes.push_back(s);
    reasons.push_back(why);
    if (why) f.clampReports.push_back({(int)i, req, s, why});
  }
  std::vector<Slot> slots;
  int pages = place(sizes, g.cols, g.rows, slots);
  if (pages > 1) {
    pages = place(sizes, g.cols, g.rows - 1, slots);
    f.pagerVisible = true;
  }
  f.pageCount = pages;
  for (size_t i = 0; i < widgets.size(); ++i) {
    Placement p;
    p.index = (int)i;
    p.type = widgets[i].type;
    p.graph = config::isGraphWidget(widgets[i]);
    p.history = !p.graph && (p.type == config::WidgetType::Sensor || p.type == config::WidgetType::BinarySensor) &&
                widgets[i].attribute.empty() && widgets[i].history.enabled && !widgets[i].entity.empty();
    p.page = slots[i].page;
    p.cellX = slots[i].c;
    p.cellY = slots[i].r;
    p.cellW = slots[i].w;
    p.cellH = slots[i].h;
    p.rect = cellRect(g, p.cellX, p.cellY, p.cellW, p.cellH);
    p.clamped = reasons[i] != 0;
    f.placements.push_back(p);
  }
  return f;
}

int tabsPerGroup(Orientation o, int tabCount) {
  int slots = gridFor(o).tabSlots;
  return tabCount <= slots ? slots : slots - 1;
}
int groupCount(Orientation o, int tabCount) {
  if (tabCount <= gridFor(o).tabSlots) return 1;
  int per = tabsPerGroup(o, tabCount);
  return (tabCount + per - 1) / per;
}
int groupForTab(Orientation o, int tabCount, int tab) {
  if (tabCount <= gridFor(o).tabSlots || tab < 0) return 0;
  return tab / tabsPerGroup(o, tabCount);
}
int nextGroup(Orientation o, int tabCount, int group) {
  int n = groupCount(o, tabCount);
  return (group + 1) % n;
}

Layout computeLayout(Orientation o, const std::vector<config::Tab> &tabs, int activeTab,
                     int subPage, int group) {
  const int T = (int)tabs.size();
  const int at = T > 0 ? std::min(std::max(activeTab, 0), T - 1) : 0;
  const Grid &g = gridFor(o, T > 0 ? tabs[at].landscapeColumns : 5);
  Layout L;
  L.orientation = o;
  L.statusBar = g.statusBar;
  L.tabBar = g.tabBar;
  L.content = g.content;
  L.refreshHit = g.refreshHit;
  L.settingsHit = g.settingsHit;
  L.sleepBadge = g.sleepBadge;
  if (T > 0) activeTab = std::min(std::max(activeTab, 0), T - 1);
  else activeTab = 0;
  L.activeTab = activeTab;
  L.groupCount = groupCount(o, T);
  L.group = group < 0 ? groupForTab(o, T, activeTab) : std::min(group, L.groupCount - 1);

  bool overflow = T > g.tabSlots;
  int per = tabsPerGroup(o, T);
  for (int i = 0; i < g.tabSlots; ++i) {
    TabSlot s;
    s.rect = g.slotRect[i];
    if (overflow && i == g.tabSlots - 1) {
      s.kind = SlotKind::Overflow;
    } else {
      int t = overflow ? L.group * per + i : i;
      if (t < T) { s.kind = SlotKind::Tab; s.tabIndex = t; }
    }
    L.tabSlots.push_back(s);
  }

  if (T > 0) L.flow = computeFlow(o, tabs[activeTab].widgets, tabs[activeTab].landscapeColumns);
  L.pager.subPageCount = L.flow.pageCount;
  L.pager.subPage = std::min(std::max(subPage, 0), L.flow.pageCount - 1);
  L.pager.visible = L.flow.pagerVisible;
  int row = g.rows - 1;
  L.pager.rect = {g.colX[0], g.rowY[row], g.content.w, kCellH};
  L.pager.prev = {L.pager.rect.x, L.pager.rect.y + 20, 120, 64};
  L.pager.next = {L.pager.rect.x + L.pager.rect.w - 120, L.pager.rect.y + 20, 120, 64};
  L.pager.label = {L.pager.rect.x + 132, L.pager.rect.y + 20, L.pager.rect.w - 264, 64};
  for (const Placement &p : L.flow.placements)
    if (p.page == L.pager.subPage) L.widgets.push_back(p);
  return L;
}

std::vector<SubRegion> subRegions(WidgetType type, WidgetSize, const Rect &r) {
  std::vector<SubRegion> out;
  const int W = r.w, y = r.y + 20;
  switch (type) {
    case WidgetType::Cover:
    case WidgetType::Vacuum:
      out.push_back({1, {r.x + W - 228, y, 64, 64}});
      out.push_back({2, {r.x + W - 152, y, 64, 64}});
      out.push_back({3, {r.x + W - 76, y, 64, 64}});
      break;
    case WidgetType::Climate:
      out.push_back({1, {r.x + W - 276, y, 64, 64}});
      out.push_back({2, {r.x + W - 76, y, 64, 64}});
      break;
    default: break;
  }
  return out;
}

Hit hitTest(const Layout &L, Point p) {
  for (const TabSlot &s : L.tabSlots) {
    if (!s.rect.contains(p)) continue;
    if (s.kind == SlotKind::Tab) return {HitKind::Tab, s.tabIndex, 0};
    if (s.kind == SlotKind::Overflow) return {HitKind::TabOverflow, 0, 0};
    return {};
  }
  if (L.refreshHit.contains(p)) return {HitKind::Refresh, 0, 0};
  if (L.settingsHit.contains(p)) return {HitKind::Settings, 0, 0};
  if (L.pager.visible) {
    if (L.pager.prev.contains(p)) return {HitKind::PagerPrev, 0, 0};
    if (L.pager.next.contains(p)) return {HitKind::PagerNext, 0, 0};
  }
  for (const Placement &w : L.widgets) {
    if (!w.rect.contains(p)) continue;
    for (const SubRegion &s : subRegions(w.type, {w.cellW, w.cellH}, w.rect))
      if (s.rect.contains(p)) return {HitKind::Widget, w.index, s.id};
    return {HitKind::Widget, w.index, 0};
  }
  return {};
}

}  // namespace ui
