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

const Grid &gridFor(Orientation o) {
  static const Grid portrait = {540, 960, 2, 5, 246,
                                {16, 278, 0, 0},
                                {56, 216, 376, 536, 696},
                                {0, 0, 540, 40}, {0, 848, 540, 112}, {16, 56, 508, 784}, 4};
  static const Grid landscape = {960, 540, 4, 3, 180,
                                 {176, 372, 568, 764},
                                 {56, 216, 376, 0, 0},
                                 {0, 0, 960, 40}, {0, 40, 160, 500}, {176, 56, 768, 464}, 5};
  return o == Orientation::Landscape ? landscape : portrait;
}

Rect cellRect(const Grid &g, int c, int r, int w, int h) {
  return {g.colX[c], g.rowY[r], w * g.cellW + (w - 1) * kGap, h * kCellH + (h - 1) * kGap};
}

SizeLimits sizeLimits(WidgetType t) {
  switch (t) {
    case WidgetType::Cover: return {{2, 1}, {2, 1}, {4, 2}};
    case WidgetType::Light:
    case WidgetType::Switch:
    case WidgetType::Automation:
    case WidgetType::Sensor:
    case WidgetType::BinarySensor: return {{1, 1}, {1, 1}, {2, 1}};
    case WidgetType::Climate: return {{2, 2}, {2, 2}, {4, 2}};
    case WidgetType::Weather: return {{2, 1}, {1, 1}, {4, 1}};
    case WidgetType::Vacuum: return {{2, 1}, {2, 1}, {4, 2}};
  }
  return {{1, 1}, {1, 1}, {1, 1}};
}

WidgetSize resolveSize(WidgetType type, const config::Widget &w, const Grid &g,
                       unsigned *reasons, WidgetSize *requested) {
  SizeLimits lim = sizeLimits(type);
  WidgetSize req = w.size ? *w.size : lim.def;
  WidgetSize s = req;
  unsigned why = 0;
  if (s.w < lim.min.w) { s.w = lim.min.w; why |= kClampTypeMin; }
  if (s.h < lim.min.h) { s.h = lim.min.h; why |= kClampTypeMin; }
  if (s.w > lim.max.w) { s.w = lim.max.w; why |= kClampTypeMax; }
  if (s.h > lim.max.h) { s.h = lim.max.h; why |= kClampTypeMax; }
  if (s.w > g.cols) { s.w = g.cols; why |= kClampColumns; }
  if (s.h > g.rows - 1) { s.h = g.rows - 1; why |= kClampRows; }
  if (reasons) *reasons = why;
  if (requested) *requested = req;
  return s;
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

Flow computeFlow(Orientation o, const std::vector<config::Widget> &widgets) {
  const Grid &g = gridFor(o);
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
  const Grid &g = gridFor(o);
  Layout L;
  L.orientation = o;
  L.statusBar = g.statusBar;
  L.tabBar = g.tabBar;
  L.content = g.content;
  int T = (int)tabs.size();
  if (T > 0) activeTab = std::min(std::max(activeTab, 0), T - 1);
  else activeTab = 0;
  L.activeTab = activeTab;
  L.groupCount = groupCount(o, T);
  L.group = group < 0 ? groupForTab(o, T, activeTab) : std::min(group, L.groupCount - 1);

  bool overflow = T > g.tabSlots;
  int per = tabsPerGroup(o, T);
  for (int i = 0; i < g.tabSlots; ++i) {
    TabSlot s;
    if (o == Orientation::Portrait) s.rect = {16 + i * 131, 864, 115, 80};
    else s.rect = {16, 56 + i * 96, 128, 80};
    if (overflow && i == g.tabSlots - 1) {
      s.kind = SlotKind::Overflow;
    } else {
      int t = overflow ? L.group * per + i : i;
      if (t < T) { s.kind = SlotKind::Tab; s.tabIndex = t; }
    }
    L.tabSlots.push_back(s);
  }

  if (T > 0) L.flow = computeFlow(o, tabs[activeTab].widgets);
  L.pager.subPageCount = L.flow.pageCount;
  L.pager.subPage = std::min(std::max(subPage, 0), L.flow.pageCount - 1);
  L.pager.visible = L.flow.pagerVisible;
  int row = g.rows - 1;
  L.pager.rect = {g.colX[0], g.rowY[row], g.content.w, kCellH};
  L.pager.prev = {L.pager.rect.x, L.pager.rect.y + 32, 120, 80};
  L.pager.next = {L.pager.rect.x + L.pager.rect.w - 120, L.pager.rect.y + 32, 120, 80};
  L.pager.label = {L.pager.rect.x + 136, L.pager.rect.y + 32, L.pager.rect.w - 272, 80};
  for (const Placement &p : L.flow.placements)
    if (p.page == L.pager.subPage) L.widgets.push_back(p);
  return L;
}

std::vector<SubRegion> subRegions(WidgetType type, WidgetSize size, const Rect &r,
                                  bool climateModeEnabled) {
  std::vector<SubRegion> out;
  switch (type) {
    case WidgetType::Cover:
    case WidgetType::Vacuum: {
      const int n = 3;
      int W = r.w;
      int avail = W - 16 - 16 * (n - 1);
      int bw = avail / n;
      int rem = avail - n * bw;
      int x0 = 8 + rem / 2;
      int y = size.h >= 2 ? 216 : 56;
      for (int i = 0; i < n; ++i) out.push_back({i + 1, {r.x + x0 + i * (bw + 16), r.y + y, bw, 80}});
      break;
    }
    case WidgetType::Climate:
      out.push_back({1, {r.x + 8, r.y + 120, 80, 80}});
      out.push_back({2, {r.x + r.w - 88, r.y + 120, 80, 80}});
      if (climateModeEnabled) out.push_back({3, {r.x + 8, r.y + 216, r.w - 16, 80}});
      break;
    default: break;
  }
  return out;
}

Hit hitTest(const Layout &L, Point p, bool climateModeEnabled) {
  for (const TabSlot &s : L.tabSlots) {
    if (!s.rect.contains(p)) continue;
    if (s.kind == SlotKind::Tab) return {HitKind::Tab, s.tabIndex, 0};
    if (s.kind == SlotKind::Overflow) return {HitKind::TabOverflow, 0, 0};
    return {};
  }
  if (L.pager.visible) {
    if (L.pager.prev.contains(p)) return {HitKind::PagerPrev, 0, 0};
    if (L.pager.next.contains(p)) return {HitKind::PagerNext, 0, 0};
  }
  for (const Placement &w : L.widgets) {
    if (!w.rect.contains(p)) continue;
    for (const SubRegion &s : subRegions(w.type, {w.cellW, w.cellH}, w.rect, climateModeEnabled))
      if (s.rect.contains(p)) return {HitKind::Widget, w.index, s.id};
    return {HitKind::Widget, w.index, 0};
  }
  return {};
}

}  // namespace ui
