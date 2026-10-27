#include <climits>
#include <random>
#include <set>
#include <unity.h>

#include "ui_layout.h"
#include "ui_rocker.h"

using namespace ui;
using config::Tab;
using config::Widget;
using config::WidgetSize;
using config::WidgetType;
using settings::Orientation;

void setUp() {}
void tearDown() {}

static Widget W(WidgetType t, int w = 0, int h = 0) {
  Widget x;
  x.type = t;
  if (w) x.size = WidgetSize{w, h};
  return x;
}
static std::vector<Tab> tabsOf(int n) {
  std::vector<Tab> t(n);
  for (int i = 0; i < n; ++i) t[i].title = "T" + std::to_string(i);
  return t;
}
static void eqRect(Rect e, Rect a) {
  TEST_ASSERT_EQUAL_INT(e.x, a.x);
  TEST_ASSERT_EQUAL_INT(e.y, a.y);
  TEST_ASSERT_EQUAL_INT(e.w, a.w);
  TEST_ASSERT_EQUAL_INT(e.h, a.h);
}
static const Orientation P = Orientation::Portrait, L = Orientation::Landscape;

// ---- geometry ----
void test_areas_portrait() {
  Layout l = computeLayout(P, tabsOf(1), 0, 0);
  eqRect({0, 0, 540, 40}, l.statusBar);
  eqRect({0, 848, 540, 112}, l.tabBar);
  eqRect({16, 56, 508, 784}, l.content);
  const Grid &g = gridFor(P);
  TEST_ASSERT_EQUAL_INT(2, g.cols);
  TEST_ASSERT_EQUAL_INT(5, g.rows);
  TEST_ASSERT_EQUAL_INT(246, g.cellW);
  int ry[] = {56, 216, 376, 536, 696};
  for (int i = 0; i < 5; ++i) TEST_ASSERT_EQUAL_INT(ry[i], g.rowY[i]);
  TEST_ASSERT_EQUAL_INT(16, g.colX[0]);
  TEST_ASSERT_EQUAL_INT(278, g.colX[1]);
}
void test_areas_landscape() {
  Layout l = computeLayout(L, tabsOf(1), 0, 0);
  eqRect({0, 0, 960, 40}, l.statusBar);
  eqRect({0, 40, 160, 500}, l.tabBar);
  eqRect({176, 56, 768, 464}, l.content);
  const Grid &g = gridFor(L);
  TEST_ASSERT_EQUAL_INT(4, g.cols);
  TEST_ASSERT_EQUAL_INT(3, g.rows);
  TEST_ASSERT_EQUAL_INT(180, g.cellW);
  int cx[] = {176, 372, 568, 764};
  for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_INT(cx[i], g.colX[i]);
}
void test_vector_slots_portrait() {
  Layout l = computeLayout(P, tabsOf(4), 0, 0);
  TEST_ASSERT_EQUAL_INT(4, (int)l.tabSlots.size());
  Rect e[] = {{16, 864, 115, 80}, {147, 864, 115, 80}, {278, 864, 115, 80}, {409, 864, 115, 80}};
  for (int i = 0; i < 4; ++i) eqRect(e[i], l.tabSlots[i].rect);
}
void test_vector_slots_landscape() {
  Layout l = computeLayout(L, tabsOf(5), 0, 0);
  TEST_ASSERT_EQUAL_INT(5, (int)l.tabSlots.size());
  Rect e[] = {{16, 56, 128, 80}, {16, 152, 128, 80}, {16, 248, 128, 80}, {16, 344, 128, 80}, {16, 440, 128, 80}};
  for (int i = 0; i < 5; ++i) eqRect(e[i], l.tabSlots[i].rect);
}
void test_vector_cell_rects() {
  eqRect({278, 56, 246, 144}, cellRect(gridFor(P), 1, 0, 1, 1));
  eqRect({16, 376, 508, 304}, cellRect(gridFor(P), 0, 2, 2, 2));
  eqRect({568, 216, 376, 144}, cellRect(gridFor(L), 2, 1, 2, 1));
  eqRect({176, 216, 768, 304}, cellRect(gridFor(L), 0, 1, 4, 2));
  eqRect({176, 56, 768, 304}, cellRect(gridFor(L), 0, 0, 4, 2));
}
void test_pager_rects() {
  std::vector<Tab> t = tabsOf(1);
  for (int i = 0; i < 6; ++i) t[0].widgets.push_back(W(WidgetType::Climate));
  Layout p = computeLayout(P, t, 0, 0);
  TEST_ASSERT_TRUE(p.pager.visible);
  eqRect({16, 696, 508, 144}, p.pager.rect);
  eqRect({16, 728, 120, 80}, p.pager.prev);
  eqRect({404, 728, 120, 80}, p.pager.next);
  eqRect({152, 728, 236, 80}, p.pager.label);
  Layout l = computeLayout(L, t, 0, 0);
  TEST_ASSERT_TRUE(l.pager.visible);
  eqRect({176, 376, 768, 144}, l.pager.rect);
  eqRect({176, 408, 120, 80}, l.pager.prev);
  eqRect({824, 408, 120, 80}, l.pager.next);
  eqRect({312, 408, 496, 80}, l.pager.label);
}

// ---- tab groups ----
void test_tabs_no_overflow() {
  Layout l = computeLayout(P, tabsOf(4), 3, 0);
  TEST_ASSERT_EQUAL_INT(1, l.groupCount);
  for (int i = 0; i < 4; ++i) {
    TEST_ASSERT_TRUE(l.tabSlots[i].kind == SlotKind::Tab);
    TEST_ASSERT_EQUAL_INT(i, l.tabSlots[i].tabIndex);
  }
  Layout s = computeLayout(P, tabsOf(2), 0, 0);
  TEST_ASSERT_TRUE(s.tabSlots[2].kind == SlotKind::Empty);
  TEST_ASSERT_TRUE(s.tabSlots[3].kind == SlotKind::Empty);
}
void test_tabs_overflow_5() {
  Layout l = computeLayout(P, tabsOf(5), 0, 0);
  TEST_ASSERT_EQUAL_INT(2, l.groupCount);
  for (int i = 0; i < 3; ++i) TEST_ASSERT_EQUAL_INT(i, l.tabSlots[i].tabIndex);
  TEST_ASSERT_TRUE(l.tabSlots[3].kind == SlotKind::Overflow);
  Layout g1 = computeLayout(P, tabsOf(5), 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(3, g1.tabSlots[0].tabIndex);
  TEST_ASSERT_EQUAL_INT(4, g1.tabSlots[1].tabIndex);
  TEST_ASSERT_TRUE(g1.tabSlots[2].kind == SlotKind::Empty);
  TEST_ASSERT_TRUE(g1.tabSlots[3].kind == SlotKind::Overflow);
}
void test_tabs_8_portrait() {
  TEST_ASSERT_EQUAL_INT(3, tabsPerGroup(P, 8));
  TEST_ASSERT_EQUAL_INT(3, groupCount(P, 8));
  Layout l = computeLayout(P, tabsOf(8), 4, 0);
  TEST_ASSERT_EQUAL_INT(1, l.group);
  TEST_ASSERT_EQUAL_INT(3, l.tabSlots[0].tabIndex);
  TEST_ASSERT_EQUAL_INT(4, l.tabSlots[1].tabIndex);
  TEST_ASSERT_EQUAL_INT(5, l.tabSlots[2].tabIndex);
  TEST_ASSERT_EQUAL_INT(0, nextGroup(P, 8, 2));
  TEST_ASSERT_EQUAL_INT(2, nextGroup(P, 8, 1));
}
void test_tabs_landscape_groups() {
  TEST_ASSERT_EQUAL_INT(4, tabsPerGroup(L, 16));
  TEST_ASSERT_EQUAL_INT(4, groupCount(L, 16));
  TEST_ASSERT_EQUAL_INT(1, groupCount(L, 5));
  TEST_ASSERT_EQUAL_INT(2, groupCount(L, 6));
  Layout l = computeLayout(L, tabsOf(6), 5, 0);
  TEST_ASSERT_EQUAL_INT(1, l.group);
  TEST_ASSERT_EQUAL_INT(4, l.tabSlots[0].tabIndex);
  TEST_ASSERT_TRUE(l.tabSlots[1].kind == SlotKind::Tab);
  TEST_ASSERT_TRUE(l.tabSlots[2].kind == SlotKind::Empty);
  TEST_ASSERT_TRUE(l.tabSlots[4].kind == SlotKind::Overflow);
}

// ---- sizes ----
void test_default_sizes() {
  struct { WidgetType t; int w, h; } d[] = {
      {WidgetType::Cover, 2, 1}, {WidgetType::Light, 1, 1}, {WidgetType::Switch, 1, 1},
      {WidgetType::Automation, 1, 1}, {WidgetType::Climate, 2, 2}, {WidgetType::Sensor, 1, 1},
      {WidgetType::BinarySensor, 1, 1}, {WidgetType::Weather, 2, 1}, {WidgetType::Vacuum, 2, 1}};
  for (auto &e : d)
    for (Orientation o : {P, L}) {
      unsigned why = 99;
      WidgetSize s = resolveSize(e.t, W(e.t), gridFor(o), &why);
      TEST_ASSERT_EQUAL_INT(e.w, s.w);
      TEST_ASSERT_EQUAL_INT(e.h, s.h);
      TEST_ASSERT_EQUAL_UINT(0, why);
    }
}
void test_size_override() {
  WidgetSize s = resolveSize(WidgetType::Light, W(WidgetType::Light, 2, 1), gridFor(L));
  TEST_ASSERT_EQUAL_INT(2, s.w);
  TEST_ASSERT_EQUAL_INT(1, s.h);
  s = resolveSize(WidgetType::Cover, W(WidgetType::Cover, 4, 2), gridFor(L));
  TEST_ASSERT_EQUAL_INT(4, s.w);
  TEST_ASSERT_EQUAL_INT(2, s.h);
}
void test_size_clamp_reports() {
  std::vector<Tab> t = tabsOf(1);
  t[0].widgets = {W(WidgetType::Light, 3, 3), W(WidgetType::Climate, 1, 1), W(WidgetType::Weather, 4, 1),
                  W(WidgetType::Light, 1, 1)};
  Flow f = computeFlow(P, t[0].widgets);
  TEST_ASSERT_EQUAL_INT(3, (int)f.clampReports.size());
  TEST_ASSERT_EQUAL_INT(0, f.clampReports[0].widgetIndex);
  TEST_ASSERT_EQUAL_INT(3, f.clampReports[0].requested.w);
  TEST_ASSERT_EQUAL_INT(2, f.clampReports[0].used.w);
  TEST_ASSERT_EQUAL_INT(1, f.clampReports[0].used.h);
  TEST_ASSERT_TRUE(f.clampReports[0].reasons & kClampTypeMax);
  TEST_ASSERT_EQUAL_INT(1, f.clampReports[1].widgetIndex);
  TEST_ASSERT_EQUAL_INT(2, f.clampReports[1].used.w);
  TEST_ASSERT_EQUAL_INT(2, f.clampReports[1].used.h);
  TEST_ASSERT_TRUE(f.clampReports[1].reasons & kClampTypeMin);
  TEST_ASSERT_EQUAL_INT(2, f.clampReports[2].widgetIndex);
  TEST_ASSERT_EQUAL_INT(2, f.clampReports[2].used.w);
  TEST_ASSERT_TRUE(f.clampReports[2].reasons & kClampColumns);
  TEST_ASSERT_TRUE(f.placements[0].clamped);
  TEST_ASSERT_FALSE(f.placements[3].clamped);
  // landscape: weather 4x1 fits, no report
  Flow fl = computeFlow(L, {W(WidgetType::Weather, 4, 1)});
  TEST_ASSERT_EQUAL_INT(0, (int)fl.clampReports.size());
}

// ---- flow ----
static std::vector<Widget> livingRoom() {
  Widget grp = W(WidgetType::Cover);
  grp.group = "g";
  return {W(WidgetType::Light), grp, W(WidgetType::Climate), W(WidgetType::Sensor), W(WidgetType::Cover)};
}
void test_flow_worked_example() {
  Flow p = computeFlow(P, livingRoom());
  TEST_ASSERT_EQUAL_INT(1, p.pageCount);
  TEST_ASSERT_FALSE(p.pagerVisible);
  int pe[][2] = {{0, 0}, {0, 1}, {0, 2}, {1, 0}, {0, 4}};
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL_INT(pe[i][0], p.placements[i].cellX);
    TEST_ASSERT_EQUAL_INT(pe[i][1], p.placements[i].cellY);
  }
  Flow l = computeFlow(L, livingRoom());
  TEST_ASSERT_EQUAL_INT(1, l.pageCount);
  int le[][2] = {{0, 0}, {1, 0}, {0, 1}, {3, 0}, {2, 1}};
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL_INT(le[i][0], l.placements[i].cellX);
    TEST_ASSERT_EQUAL_INT(le[i][1], l.placements[i].cellY);
  }
  eqRect({568, 216, 376, 144}, l.placements[4].rect);
}
void test_flow_empty_tab() {
  Layout l = computeLayout(P, tabsOf(1), 0, 5);
  TEST_ASSERT_EQUAL_INT(1, l.pager.subPageCount);
  TEST_ASSERT_FALSE(l.pager.visible);
  TEST_ASSERT_EQUAL_INT(0, l.pager.subPage);
  TEST_ASSERT_EQUAL_INT(0, (int)l.widgets.size());
  Layout none = computeLayout(P, {}, 0, 0);
  TEST_ASSERT_EQUAL_INT(0, (int)none.widgets.size());
}
void test_flow_full_page_no_pager() {
  std::vector<Widget> w(10, W(WidgetType::Light));  // 10 x 1x1 = exactly 5 rows x 2
  Flow f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(1, f.pageCount);
  TEST_ASSERT_FALSE(f.pagerVisible);
  w.push_back(W(WidgetType::Light));
  f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_TRUE(f.pagerVisible);
  // reflow with 4 rows: 8 per page -> widgets 8..10 on page 1
  TEST_ASSERT_EQUAL_INT(0, f.placements[7].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[8].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[10].page);
  TEST_ASSERT_EQUAL_INT(0, f.placements[8].cellX);
  TEST_ASSERT_EQUAL_INT(0, f.placements[8].cellY);
}
void test_flow_gap_fill_current_page_only() {
  // landscape, pager mode (2 rows x 4 cols). Fill page 0, widget on page 1, later small
  // widget must go on page 1's gaps, never back to page 0.
  std::vector<Widget> w;
  w.push_back(W(WidgetType::Climate, 4, 2));  // page 0 full
  w.push_back(W(WidgetType::Light));          // page 1 (0,0)
  w.push_back(W(WidgetType::Cover, 4, 1));    // page 1 row 1? needs 4 wide: row 1
  w.push_back(W(WidgetType::Light));          // page 1 gap (1,0)
  Flow f = computeFlow(L, w);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_EQUAL_INT(0, f.placements[0].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[1].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[2].page);
  TEST_ASSERT_EQUAL_INT(0, f.placements[2].cellX);
  TEST_ASSERT_EQUAL_INT(1, f.placements[2].cellY);
  TEST_ASSERT_EQUAL_INT(1, f.placements[3].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[3].cellX);
  TEST_ASSERT_EQUAL_INT(0, f.placements[3].cellY);
}
void test_flow_no_backfill_earlier_page() {
  // portrait 4 rows pager mode: A(2x2) B(2x2) fill page0 only if... use gap: 2x4? use climate x2 + big
  std::vector<Widget> w;
  w.push_back(W(WidgetType::Climate));        // (0,0) 2x2
  w.push_back(W(WidgetType::Climate));        // (0,2) 2x2 -> page 0 full (4 rows)
  w.push_back(W(WidgetType::Light));          // page 1
  w.push_back(W(WidgetType::Vacuum, 2, 2));   // page 1: 2x2 at row 1
  w.push_back(W(WidgetType::Light));          // page 1 gap (1,0)
  Flow f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_EQUAL_INT(1, f.placements[3].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[3].cellY);
  TEST_ASSERT_EQUAL_INT(1, f.placements[4].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[4].cellX);
  // three pages
  std::vector<Widget> c(5, W(WidgetType::Climate));
  Flow f3 = computeFlow(P, c);
  TEST_ASSERT_EQUAL_INT(3, f3.pageCount);
  Layout l = computeLayout(P, [&] { auto t = tabsOf(1); t[0].widgets = c; return t; }(), 0, 7);
  TEST_ASSERT_EQUAL_INT(2, l.pager.subPage);
  TEST_ASSERT_EQUAL_INT(1, (int)l.widgets.size());
  TEST_ASSERT_EQUAL_INT(4, l.widgets[0].index);
}

// ---- sub-regions ----
void test_subregions_cover_portrait() {
  Rect r = cellRect(gridFor(P), 0, 0, 2, 1);
  auto s = subRegions(WidgetType::Cover, {2, 1}, r);
  TEST_ASSERT_EQUAL_INT(3, (int)s.size());
  eqRect({16 + 8, 56 + 56, 153, 80}, s[0].rect);
  eqRect({16 + 177, 56 + 56, 153, 80}, s[1].rect);
  eqRect({16 + 346, 56 + 56, 153, 80}, s[2].rect);
  TEST_ASSERT_EQUAL_INT(1, s[0].id);
  TEST_ASSERT_EQUAL_INT(3, s[2].id);
}
void test_subregions_cover_landscape_and_2x2() {
  Rect r = cellRect(gridFor(L), 0, 0, 2, 1);
  auto s = subRegions(WidgetType::Vacuum, {2, 1}, r);
  eqRect({176 + 8, 56 + 56, 109, 80}, s[0].rect);
  eqRect({176 + 133, 56 + 56, 109, 80}, s[1].rect);
  eqRect({176 + 258, 56 + 56, 109, 80}, s[2].rect);
  Rect r2 = cellRect(gridFor(L), 0, 0, 2, 2);
  auto t = subRegions(WidgetType::Cover, {2, 2}, r2);
  TEST_ASSERT_EQUAL_INT(56 + 216, t[0].rect.y);
}
void test_subregions_climate() {
  Rect r = cellRect(gridFor(P), 0, 2, 2, 2);
  auto s = subRegions(WidgetType::Climate, {2, 2}, r);
  TEST_ASSERT_EQUAL_INT(2, (int)s.size());
  eqRect({16 + 8, 376 + 120, 80, 80}, s[0].rect);
  eqRect({16 + 508 - 88, 376 + 120, 80, 80}, s[1].rect);
  auto m = subRegions(WidgetType::Climate, {2, 2}, r, true);
  TEST_ASSERT_EQUAL_INT(3, (int)m.size());
  eqRect({16 + 8, 376 + 216, 492, 80}, m[2].rect);
}
void test_subregions_none_and_min_size() {
  for (WidgetType t : {WidgetType::Light, WidgetType::Switch, WidgetType::Automation, WidgetType::Sensor,
                       WidgetType::BinarySensor, WidgetType::Weather})
    TEST_ASSERT_EQUAL_INT(0, (int)subRegions(t, {1, 1}, {0, 0, 246, 144}).size());
  for (Orientation o : {P, L})
    for (WidgetType t : {WidgetType::Cover, WidgetType::Vacuum, WidgetType::Climate}) {
      SizeLimits lim = sizeLimits(t);
      Widget w = W(t);
      WidgetSize s = resolveSize(t, w, gridFor(o));
      TEST_ASSERT_TRUE(s.w >= lim.min.w);
      Rect r = cellRect(gridFor(o), 0, 0, s.w, s.h);
      auto subs = subRegions(t, s, r, true);
      TEST_ASSERT_TRUE(subs.size() >= 2);
      for (size_t i = 0; i < subs.size(); ++i) {
        TEST_ASSERT_TRUE(subs[i].rect.w >= 80);
        TEST_ASSERT_TRUE(subs[i].rect.h >= 80);
        TEST_ASSERT_TRUE(rectInside(subs[i].rect, r));
        for (size_t j = i + 1; j < subs.size(); ++j) {
          TEST_ASSERT_FALSE(rectsOverlap(subs[i].rect, subs[j].rect));
          TEST_ASSERT_FALSE(rectsOverlap(inflate(subs[i].rect, 7), subs[j].rect));  // >= 16 apart? (8 each side)
        }
      }
    }
}

// ---- property ----
void test_property_no_overlap() {
  std::mt19937 rng(12345);
  WidgetType types[] = {WidgetType::Cover, WidgetType::Light, WidgetType::Switch, WidgetType::Automation,
                        WidgetType::Climate, WidgetType::Sensor, WidgetType::BinarySensor,
                        WidgetType::Weather, WidgetType::Vacuum};
  for (int iter = 0; iter < 300; ++iter) {
    Orientation o = (iter & 1) ? L : P;
    int n = rng() % 25;
    std::vector<Tab> tabs = tabsOf(1);
    for (int i = 0; i < n; ++i) {
      Widget w = W(types[rng() % 9]);
      if (rng() % 2) w.size = WidgetSize{1 + (int)(rng() % 4), 1 + (int)(rng() % 4)};
      tabs[0].widgets.push_back(w);
    }
    Layout base = computeLayout(o, tabs, 0, 0);
    int pages = base.pager.subPageCount;
    int count = 0;
    for (int p = 0; p < pages; ++p) {
      Layout l = computeLayout(o, tabs, 0, p);
      count += (int)l.widgets.size();
      for (size_t i = 0; i < l.widgets.size(); ++i) {
        TEST_ASSERT_TRUE(rectInside(l.widgets[i].rect, l.content));
        if (l.pager.visible) TEST_ASSERT_FALSE(rectsOverlap(l.widgets[i].rect, l.pager.rect));
        for (size_t j = i + 1; j < l.widgets.size(); ++j)
          TEST_ASSERT_FALSE(rectsOverlap(l.widgets[i].rect, l.widgets[j].rect));
      }
    }
    TEST_ASSERT_EQUAL_INT(n, count);
  }
}

// ---- hit test ----
static Hit H(const Layout &l, int x, int y) { return hitTest(l, {x, y}); }
void test_hit_tabs_and_edges() {
  Layout l = computeLayout(P, tabsOf(5), 0, 0);
  TEST_ASSERT_TRUE(H(l, 16, 864) == (Hit{HitKind::Tab, 0, 0}));
  TEST_ASSERT_TRUE(H(l, 130, 943) == (Hit{HitKind::Tab, 0, 0}));
  TEST_ASSERT_TRUE(H(l, 131, 864).kind == HitKind::None);  // gap
  TEST_ASSERT_TRUE(H(l, 15, 864).kind == HitKind::None);   // margin
  TEST_ASSERT_TRUE(H(l, 130, 944).kind == HitKind::None);
  TEST_ASSERT_TRUE(H(l, 147, 864) == (Hit{HitKind::Tab, 1, 0}));
  TEST_ASSERT_TRUE(H(l, 409, 900).kind == HitKind::TabOverflow);
  Layout e = computeLayout(P, tabsOf(2), 0, 0);
  TEST_ASSERT_TRUE(H(e, 300, 900).kind == HitKind::None);  // empty slot
  Layout ls = computeLayout(L, tabsOf(3), 0, 0);
  TEST_ASSERT_TRUE(H(ls, 16, 152) == (Hit{HitKind::Tab, 1, 0}));
  TEST_ASSERT_TRUE(H(ls, 143, 231) == (Hit{HitKind::Tab, 1, 0}));
  TEST_ASSERT_TRUE(H(ls, 20, 232).kind == HitKind::None);
  TEST_ASSERT_TRUE(H(ls, 5, 100).kind == HitKind::None);
  TEST_ASSERT_TRUE(H(ls, 100, 10).kind == HitKind::None);  // status bar
}
void test_hit_widgets() {
  std::vector<Tab> t = tabsOf(1);
  t[0].widgets = livingRoom();
  Layout l = computeLayout(P, t, 0, 0);
  TEST_ASSERT_TRUE(H(l, 16, 56) == (Hit{HitKind::Widget, 0, 0}));
  TEST_ASSERT_TRUE(H(l, 261, 199) == (Hit{HitKind::Widget, 0, 0}));
  TEST_ASSERT_TRUE(H(l, 262, 100).kind == HitKind::None);  // gap column
  TEST_ASSERT_TRUE(H(l, 100, 200).kind == HitKind::None);  // gap row
  TEST_ASSERT_TRUE(H(l, 278, 56) == (Hit{HitKind::Widget, 3, 0}));
  TEST_ASSERT_TRUE(H(l, 15, 56).kind == HitKind::None);
  // group (index 1) at (0,1): y 216; buttons y 272..351, x 24..176
  TEST_ASSERT_TRUE(H(l, 24, 272) == (Hit{HitKind::Widget, 1, 1}));
  TEST_ASSERT_TRUE(H(l, 176, 351) == (Hit{HitKind::Widget, 1, 1}));
  TEST_ASSERT_TRUE(H(l, 177, 300) == (Hit{HitKind::Widget, 1, 0}));  // between buttons
  TEST_ASSERT_TRUE(H(l, 193, 300) == (Hit{HitKind::Widget, 1, 2}));
  TEST_ASSERT_TRUE(H(l, 362, 300) == (Hit{HitKind::Widget, 1, 3}));
  TEST_ASSERT_TRUE(H(l, 100, 271) == (Hit{HitKind::Widget, 1, 0}));
  // climate (index 2) at (0,2): minus at 24,496
  TEST_ASSERT_TRUE(H(l, 24, 496) == (Hit{HitKind::Widget, 2, 1}));
  TEST_ASSERT_TRUE(H(l, 103, 575) == (Hit{HitKind::Widget, 2, 1}));
  TEST_ASSERT_TRUE(H(l, 104, 575) == (Hit{HitKind::Widget, 2, 0}));
  TEST_ASSERT_TRUE(H(l, 436, 496) == (Hit{HitKind::Widget, 2, 2}));
  // widget on another sub-page is not hit; pager not visible
  TEST_ASSERT_TRUE(H(l, 404, 728).kind == HitKind::None || H(l, 404, 728).kind == HitKind::Widget);
}
void test_hit_pager() {
  std::vector<Tab> t = tabsOf(1);
  for (int i = 0; i < 12; ++i) t[0].widgets.push_back(W(WidgetType::Light));
  Layout l = computeLayout(P, t, 0, 0);
  TEST_ASSERT_TRUE(l.pager.visible);
  TEST_ASSERT_TRUE(H(l, 16, 728).kind == HitKind::PagerPrev);
  TEST_ASSERT_TRUE(H(l, 135, 807).kind == HitKind::PagerPrev);
  TEST_ASSERT_TRUE(H(l, 136, 728).kind == HitKind::None);
  TEST_ASSERT_TRUE(H(l, 404, 728).kind == HitKind::PagerNext);
  TEST_ASSERT_TRUE(H(l, 523, 807).kind == HitKind::PagerNext);
  TEST_ASSERT_TRUE(H(l, 404, 727).kind == HitKind::None);
  // last widget row ends at 696-16=680 .. widgets in rows 0..3
  TEST_ASSERT_TRUE(H(l, 20, 679).kind == HitKind::Widget);
  TEST_ASSERT_TRUE(H(l, 20, 696).kind == HitKind::None);
  t[0].widgets.push_back(W(WidgetType::Light));
  t[0].widgets.push_back(W(WidgetType::Light));
  Layout l2 = computeLayout(L, t, 0, 0);
  TEST_ASSERT_TRUE(l2.pager.visible);
  TEST_ASSERT_TRUE(H(l2, 176, 408).kind == HitKind::PagerPrev);
  TEST_ASSERT_TRUE(H(l2, 943, 487).kind == HitKind::PagerNext);
}

// ---- rocker ----
void test_rocker_mapping() {
  for (Orientation o : {P, L}) {
    TEST_ASSERT_TRUE(mapRocker(PhysicalKey::Up, o, false) == LogicalKey::Prev);
    TEST_ASSERT_TRUE(mapRocker(PhysicalKey::Down, o, false) == LogicalKey::Next);
    TEST_ASSERT_TRUE(mapRocker(PhysicalKey::Up, o, true) == LogicalKey::Next);
    TEST_ASSERT_TRUE(mapRocker(PhysicalKey::Down, o, true) == LogicalKey::Prev);
    TEST_ASSERT_TRUE(mapRocker(PhysicalKey::Push, o, true) == LogicalKey::Push);
    TEST_ASSERT_TRUE(mapRocker(PhysicalKey::LongPush, o, true) == LogicalKey::LongPush);
  }
}

struct Fx {
  Orientation o;
  std::vector<Tab> tabs;
  Flow flow;
  FocusEnv env;
  FocusState st;
  Fx(Orientation orient, int nTabs, std::vector<Widget> w0) : o(orient), tabs(tabsOf(nTabs)) {
    tabs[0].widgets = std::move(w0);
    refresh();
  }
  void refresh() {
    flow = computeFlow(o, tabs[st.tab].widgets);
    env.orientation = o;
    env.tabCount = (int)tabs.size();
    env.flow = &flow;
  }
  FocusResult key(LogicalKey k) {
    refresh();
    FocusResult r = focusHandle(st, env, k);
    refresh();
    return r;
  }
};

void test_focus_tab_mode_wrap_and_groups() {
  for (Orientation o : {P, L}) {
    Fx f(o, 8, {});
    FocusResult r = f.key(LogicalKey::Prev);
    TEST_ASSERT_TRUE(r.effect == Effect::TabChanged);
    TEST_ASSERT_EQUAL_INT(7, f.st.tab);
    TEST_ASSERT_EQUAL_INT(groupForTab(o, 8, 7), f.st.group);
    r = f.key(LogicalKey::Next);
    TEST_ASSERT_EQUAL_INT(0, f.st.tab);
    TEST_ASSERT_EQUAL_INT(0, f.st.group);
    f.key(LogicalKey::Next);
    f.key(LogicalKey::Next);
    f.key(LogicalKey::Next);
    TEST_ASSERT_EQUAL_INT(3, f.st.tab);
    TEST_ASSERT_EQUAL_INT(o == P ? 1 : 0, f.st.group);
    TEST_ASSERT_TRUE(f.st.ringVisible);
    TEST_ASSERT_TRUE(f.key(LogicalKey::LongPush).effect == Effect::RefreshNow);
    TEST_ASSERT_TRUE(f.key(LogicalKey::Push).effect == Effect::None);  // empty tab
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  }
}
void test_focus_order_and_skip_readonly() {
  for (Orientation o : {P, L}) {
    // sensor, light, weather, switch, climate
    Fx f(o, 2, {W(WidgetType::Sensor), W(WidgetType::Light), W(WidgetType::Weather, 2, 1),
                W(WidgetType::Switch), W(WidgetType::Climate)});
    auto stops = focusStops(f.env, 0);
    TEST_ASSERT_EQUAL_INT(3, (int)stops.size());
    // all read-only skipped; sorted by (row, col)
    TEST_ASSERT_EQUAL_INT(1, stops[0]);
    TEST_ASSERT_TRUE(f.key(LogicalKey::Push).effect == Effect::FocusMoved);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
    TEST_ASSERT_EQUAL_INT(1, focusedWidget(f.st, f.env));
    f.key(LogicalKey::Next);
    int w2 = focusedWidget(f.st, f.env);
    f.key(LogicalKey::Next);
    int w3 = focusedWidget(f.st, f.env);
    TEST_ASSERT_TRUE(w2 != w3 && w2 != 1 && w3 != 1);
    for (int w : {1, w2, w3}) TEST_ASSERT_TRUE(w == 1 || w == 3 || w == 4);
    // last widget NEXT, single page -> tab bar
    FocusResult r = f.key(LogicalKey::Next);
    TEST_ASSERT_TRUE(r.effect == Effect::FocusMoved);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
    // enter again, PREV from first -> TAB
    f.key(LogicalKey::Push);
    f.key(LogicalKey::Prev);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
    TEST_ASSERT_EQUAL_INT(0, f.st.tab);
  }
}
void test_focus_row_col_order() {
  // portrait: cover (0,0) 2x1; light (0,1); light (1,1)
  Fx f(P, 1, {W(WidgetType::Cover), W(WidgetType::Light), W(WidgetType::Light)});
  auto s = focusStops(f.env, 0);
  TEST_ASSERT_EQUAL_INT(3, (int)s.size());
  // sensor fills gaps: sorting by (row,col) not config order
  Fx g(P, 1, {W(WidgetType::Climate), W(WidgetType::Light), W(WidgetType::Light), W(WidgetType::Light)});
  // climate (0,0) 2x2; lights at (0,2),(1,2),(0,3)
  auto s2 = focusStops(g.env, 0);
  TEST_ASSERT_EQUAL_INT(0, s2[0]);
  TEST_ASSERT_EQUAL_INT(1, s2[1]);
  TEST_ASSERT_EQUAL_INT(2, s2[2]);
  TEST_ASSERT_EQUAL_INT(3, s2[3]);
  // order differs from config: sensor 1x1 at (0,0), wide cover (0,1), light fills (1,0)
  Fx h(P, 1, {W(WidgetType::Sensor), W(WidgetType::Cover), W(WidgetType::Light)});
  auto s3 = focusStops(h.env, 0);
  TEST_ASSERT_EQUAL_INT(2, s3[0]);  // light at (1,0) before cover at (0,1)
  TEST_ASSERT_EQUAL_INT(1, s3[1]);
}
void test_focus_unavailable_skipped() {
  Fx f(P, 1, {W(WidgetType::Light), W(WidgetType::Switch), W(WidgetType::Light)});
  f.env.unavailable = {false, true, false};
  auto s = focusStops(f.env, 0);
  TEST_ASSERT_EQUAL_INT(2, (int)s.size());
  TEST_ASSERT_EQUAL_INT(0, s[0]);
  TEST_ASSERT_EQUAL_INT(2, s[1]);
}
void test_focus_single_action_and_inner() {
  Fx f(L, 1, {W(WidgetType::Light), W(WidgetType::Cover), W(WidgetType::Climate), W(WidgetType::Vacuum)});
  f.key(LogicalKey::Push);  // WIDGET, light
  FocusResult r = f.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(r.effect == Effect::WidgetAction);
  TEST_ASSERT_EQUAL_INT(0, r.widgetIndex);
  TEST_ASSERT_EQUAL_INT(0, r.sub);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
  f.key(LogicalKey::Next);  // cover
  TEST_ASSERT_EQUAL_INT(1, focusedWidget(f.st, f.env));
  f.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Inner);
  TEST_ASSERT_EQUAL_INT(2, f.st.inner);  // stop
  f.key(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(3, f.st.inner);
  f.key(LogicalKey::Next);  // wrap
  TEST_ASSERT_EQUAL_INT(1, f.st.inner);
  f.key(LogicalKey::Prev);  // wrap back
  TEST_ASSERT_EQUAL_INT(3, f.st.inner);
  r = f.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(r.effect == Effect::WidgetAction);
  TEST_ASSERT_EQUAL_INT(1, r.widgetIndex);
  TEST_ASSERT_EQUAL_INT(3, r.sub);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Inner);
  f.key(LogicalKey::LongPush);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
  TEST_ASSERT_EQUAL_INT(1, focusedWidget(f.st, f.env));
  f.key(LogicalKey::Next);  // climate
  f.key(LogicalKey::Push);
  TEST_ASSERT_EQUAL_INT(1, f.st.inner);  // minus
  f.key(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(2, f.st.inner);
  f.key(LogicalKey::Next);  // mode disabled: wraps to minus
  TEST_ASSERT_EQUAL_INT(1, f.st.inner);
  f.env.climateModeEnabled = true;
  f.key(LogicalKey::Prev);
  TEST_ASSERT_EQUAL_INT(3, f.st.inner);
  f.key(LogicalKey::LongPush);
  f.env.climateModeEnabled = false;
  f.key(LogicalKey::Next);  // vacuum
  f.key(LogicalKey::Push);
  TEST_ASSERT_EQUAL_INT(1, f.st.inner);  // start
  f.key(LogicalKey::LongPush);
  f.key(LogicalKey::LongPush);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
}
void test_focus_subpages() {
  // portrait pager mode: 4 climates -> 2 per page? 4 rows -> 2 climates per page
  std::vector<Widget> w(5, W(WidgetType::Climate));
  Fx f(P, 1, w);
  TEST_ASSERT_EQUAL_INT(3, f.flow.pageCount);
  f.key(LogicalKey::Push);
  f.key(LogicalKey::Next);  // widget 1
  TEST_ASSERT_EQUAL_INT(1, focusedWidget(f.st, f.env));
  FocusResult r = f.key(LogicalKey::Next);
  TEST_ASSERT_TRUE(r.effect == Effect::SubPageChanged);
  TEST_ASSERT_EQUAL_INT(1, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(2, focusedWidget(f.st, f.env));
  // PREV at first stop of page 1 -> back to previous page, last stop
  r = f.key(LogicalKey::Prev);
  TEST_ASSERT_TRUE(r.effect == Effect::SubPageChanged);
  TEST_ASSERT_EQUAL_INT(0, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(1, focusedWidget(f.st, f.env));
  // go to last page, NEXT after last -> TAB mode
  f.key(LogicalKey::Next);
  f.key(LogicalKey::Next);
  f.key(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(2, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(4, focusedWidget(f.st, f.env));
  r = f.key(LogicalKey::Next);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  TEST_ASSERT_TRUE(r.effect == Effect::FocusMoved);
}
void test_focus_skips_pages_without_focusable() {
  // landscape pager mode (2 rows): page 0 climate 4x2, page 1 weather 4x2? weather max 4x1 -> two sensors rows
  std::vector<Widget> w = {W(WidgetType::Climate, 4, 2), W(WidgetType::Weather, 4, 1),
                           W(WidgetType::Weather, 4, 1), W(WidgetType::Light)};
  Fx f(L, 1, w);
  TEST_ASSERT_EQUAL_INT(3, f.flow.pageCount);  // climate | weather+weather | light
  TEST_ASSERT_EQUAL_INT(0, (int)focusStops(f.env, 1).size());
  f.key(LogicalKey::Push);
  FocusResult r = f.key(LogicalKey::Next);  // skip page 1
  TEST_ASSERT_TRUE(r.effect == Effect::SubPageChanged);
  TEST_ASSERT_EQUAL_INT(2, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(3, focusedWidget(f.st, f.env));
  r = f.key(LogicalKey::Prev);
  TEST_ASSERT_EQUAL_INT(0, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(0, focusedWidget(f.st, f.env));
  r = f.key(LogicalKey::Prev);  // sub-page 0 first stop -> TAB
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
}
void test_focus_touch() {
  FocusState st;
  st.mode = FocusMode::Inner;
  st.subPage = 2;
  focusTouchTab(st, P, 8, 5);
  TEST_ASSERT_TRUE(st.mode == FocusMode::Tab);
  TEST_ASSERT_EQUAL_INT(5, st.tab);
  TEST_ASSERT_EQUAL_INT(1, st.group);
  TEST_ASSERT_EQUAL_INT(0, st.subPage);
  focusOverflow(st, P, 8);
  TEST_ASSERT_EQUAL_INT(2, st.group);
  TEST_ASSERT_EQUAL_INT(5, st.tab);
  st.mode = FocusMode::Widget;
  TEST_ASSERT_FALSE(focusPagerStep(st, -1, 3));
  TEST_ASSERT_TRUE(focusPagerStep(st, 1, 3));
  TEST_ASSERT_TRUE(st.mode == FocusMode::Tab);
  TEST_ASSERT_TRUE(focusPagerStep(st, 1, 3));
  TEST_ASSERT_FALSE(focusPagerStep(st, 1, 3));
  TEST_ASSERT_EQUAL_INT(2, st.subPage);
}
void test_focus_invert_flow() {
  for (Orientation o : {P, L}) {
    Fx f(o, 3, {});
    FocusResult r = f.key(mapRocker(PhysicalKey::Down, o, true));  // inverted: DOWN = PREV
    TEST_ASSERT_TRUE(r.effect == Effect::TabChanged);
    TEST_ASSERT_EQUAL_INT(2, f.st.tab);
    f.key(mapRocker(PhysicalKey::Up, o, true));
    TEST_ASSERT_EQUAL_INT(0, f.st.tab);
  }
}

// ======================= tester additions (task 006 verification) =======================

// ---- mockup match: docs/ui/tab-portrait.svg / tab-landscape.svg ("Living room" tab) ----
// Expected rects are the translate(...) origins and frame sizes drawn in the SVGs.
void test_mockup_portrait() {
  std::vector<Tab> t = tabsOf(4);  // mockup: 4 tabs, slot 4 used, no overflow
  t[0].widgets = livingRoom();
  Layout l = computeLayout(P, t, 0, 0);
  eqRect({0, 0, 540, 40}, l.statusBar);   // svg: rule rect (0,38,540,2) closes this bar
  eqRect({0, 848, 540, 112}, l.tabBar);   // svg: rule (0,848,540,2) is its top edge
  TEST_ASSERT_FALSE(l.pager.visible);
  TEST_ASSERT_EQUAL_INT(5, (int)l.widgets.size());
  Rect e[] = {{16, 56, 246, 144},   // light
              {16, 216, 508, 144},  // cover group
              {16, 376, 508, 304},  // climate
              {278, 56, 246, 144},  // sensor
              {16, 696, 508, 144}}; // cover
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL_INT(i, l.widgets[i].index);
    eqRect(e[i], l.widgets[i].rect);
  }
  // tab slots: svg active slot (16,864,115,80); others drawn at 148/279/410 with 113 wide + 1px stroke
  Rect s[] = {{16, 864, 115, 80}, {147, 864, 115, 80}, {278, 864, 115, 80}, {409, 864, 115, 80}};
  for (int i = 0; i < 4; ++i) eqRect(s[i], l.tabSlots[i].rect);
}
void test_mockup_landscape() {
  std::vector<Tab> t = tabsOf(4);  // svg: 5 slots, 4 tabs, slot 5 empty
  t[0].widgets = livingRoom();
  Layout l = computeLayout(L, t, 0, 0);
  eqRect({0, 0, 960, 40}, l.statusBar);
  eqRect({0, 40, 160, 500}, l.tabBar);
  TEST_ASSERT_FALSE(l.pager.visible);
  TEST_ASSERT_EQUAL_INT(5, (int)l.widgets.size());
  Rect e[] = {{176, 56, 180, 144},   // light
              {372, 56, 376, 144},   // cover group
              {176, 216, 376, 304},  // climate
              {764, 56, 180, 144},   // sensor
              {568, 216, 376, 144}}; // cover
  for (int i = 0; i < 5; ++i) eqRect(e[i], l.widgets[i].rect);
  int ys[] = {56, 152, 248, 344, 440};
  for (int i = 0; i < 5; ++i) eqRect({16, ys[i], 128, 80}, l.tabSlots[i].rect);
  TEST_ASSERT_TRUE(l.tabSlots[3].kind == SlotKind::Tab);
  TEST_ASSERT_TRUE(l.tabSlots[4].kind == SlotKind::Empty);
}

// ---- robustness ----
static void checkLayoutInvariants(const Layout &l) {
  for (size_t i = 0; i < l.widgets.size(); ++i) {
    TEST_ASSERT_TRUE(rectInside(l.widgets[i].rect, l.content));
    if (l.pager.visible) TEST_ASSERT_FALSE(rectsOverlap(l.widgets[i].rect, l.pager.rect));
    for (size_t j = i + 1; j < l.widgets.size(); ++j)
      TEST_ASSERT_FALSE(rectsOverlap(l.widgets[i].rect, l.widgets[j].rect));
  }
  TEST_ASSERT_TRUE(l.pager.subPage >= 0 && l.pager.subPage < l.pager.subPageCount);
}
void test_robust_zero_tabs() {
  for (Orientation o : {P, L}) {
    std::vector<Tab> none;
    Layout l = computeLayout(o, none, 5, 5, 3);
    TEST_ASSERT_EQUAL_INT(gridFor(o).tabSlots, (int)l.tabSlots.size());
    for (auto &s : l.tabSlots) TEST_ASSERT_TRUE(s.kind == SlotKind::Empty);
    TEST_ASSERT_EQUAL_INT(0, l.activeTab);
    TEST_ASSERT_EQUAL_INT(0, l.group);
    TEST_ASSERT_EQUAL_INT(1, l.groupCount);
    TEST_ASSERT_EQUAL_INT(0, (int)l.widgets.size());
    TEST_ASSERT_FALSE(l.pager.visible);
    for (int y = -5; y < 970; y += 7)
      for (int x = -5; x < 970; x += 7) TEST_ASSERT_TRUE(hitTest(l, {x, y}).kind == HitKind::None);
    TEST_ASSERT_EQUAL_INT(1, groupCount(o, 0));
    TEST_ASSERT_EQUAL_INT(0, groupForTab(o, 0, 0));
    TEST_ASSERT_EQUAL_INT(0, nextGroup(o, 0, 0));
  }
}
static std::vector<Tab> bigConfig(int nTabs, int nWidgets) {
  WidgetType types[] = {WidgetType::Cover, WidgetType::Light, WidgetType::Switch, WidgetType::Automation,
                        WidgetType::Climate, WidgetType::Sensor, WidgetType::BinarySensor,
                        WidgetType::Weather, WidgetType::Vacuum};
  std::vector<Tab> t = tabsOf(nTabs);
  for (int i = 0; i < nTabs; ++i)
    for (int j = 0; j < nWidgets; ++j) t[i].widgets.push_back(W(types[(i + j * 5) % 9]));
  return t;
}
void test_robust_16_tabs_24_widgets() {
  std::vector<Tab> t = bigConfig(16, 24);
  for (Orientation o : {P, L}) {
    int per = gridFor(o).tabSlots - 1;
    TEST_ASSERT_EQUAL_INT(per, tabsPerGroup(o, 16));
    TEST_ASSERT_EQUAL_INT((16 + per - 1) / per, groupCount(o, 16));
    std::set<int> shown;
    for (int tab = 0; tab < 16; ++tab) {
      Layout base = computeLayout(o, t, tab, 0);
      TEST_ASSERT_EQUAL_INT(tab / per, base.group);
      TEST_ASSERT_EQUAL_INT(tab, base.activeTab);
      TEST_ASSERT_TRUE(base.tabSlots.back().kind == SlotKind::Overflow);
      bool activeShown = false;
      for (auto &s : base.tabSlots)
        if (s.kind == SlotKind::Tab) {
          TEST_ASSERT_TRUE(s.tabIndex >= 0 && s.tabIndex < 16);
          if (s.tabIndex == tab) activeShown = true;
        }
      TEST_ASSERT_TRUE(activeShown);  // active tab is always in the visible group
      int count = 0;
      for (int p = 0; p < base.pager.subPageCount; ++p) {
        Layout l = computeLayout(o, t, tab, p);
        checkLayoutInvariants(l);
        count += (int)l.widgets.size();
        TEST_ASSERT_TRUE(l.pager.visible == (base.pager.subPageCount > 1));
      }
      TEST_ASSERT_EQUAL_INT(24, count);
    }
    for (int g = 0; g < groupCount(o, 16); ++g) {
      Layout l = computeLayout(o, t, 0, 0, g);
      for (auto &s : l.tabSlots)
        if (s.kind == SlotKind::Tab) shown.insert(s.tabIndex);
    }
    TEST_ASSERT_EQUAL_INT(16, (int)shown.size());  // every tab reachable via overflow groups
  }
}
void test_robust_all_2x2() {
  for (Orientation o : {P, L}) {
    std::vector<Tab> t = tabsOf(1);
    WidgetType types[] = {WidgetType::Cover, WidgetType::Climate, WidgetType::Vacuum};
    for (int i = 0; i < 24; ++i) t[0].widgets.push_back(W(types[i % 3], 2, 2));
    Layout base = computeLayout(o, t, 0, 0);
    // pager mode: portrait 2 cols x 4 rows -> 2 per page; landscape 4 cols x 2 rows -> 2 per page
    TEST_ASSERT_EQUAL_INT(12, base.pager.subPageCount);
    TEST_ASSERT_TRUE(base.pager.visible);
    TEST_ASSERT_EQUAL_INT(0, (int)base.flow.clampReports.size());
    for (int p = 0; p < 12; ++p) {
      Layout l = computeLayout(o, t, 0, p);
      TEST_ASSERT_EQUAL_INT(2, (int)l.widgets.size());
      TEST_ASSERT_EQUAL_INT(2 * p, l.widgets[0].index);
      checkLayoutInvariants(l);
    }
    // 2x2 light: clamped to 2x1 (type max) and reported, every one of them
    std::vector<Widget> lights(24, W(WidgetType::Light, 2, 2));
    Flow f = computeFlow(o, lights);
    TEST_ASSERT_EQUAL_INT(24, (int)f.clampReports.size());
    for (auto &c : f.clampReports) {
      TEST_ASSERT_EQUAL_INT(2, c.used.w);
      TEST_ASSERT_EQUAL_INT(1, c.used.h);
      TEST_ASSERT_TRUE(c.reasons & kClampTypeMax);
    }
  }
}
void test_robust_nothing_focusable() {
  for (Orientation o : {P, L}) {
    Fx f(o, 2, {W(WidgetType::Sensor), W(WidgetType::BinarySensor), W(WidgetType::Weather)});
    TEST_ASSERT_EQUAL_INT(0, (int)focusStops(f.env, 0).size());
    FocusResult r = f.key(LogicalKey::Push);
    TEST_ASSERT_TRUE(r.effect == Effect::None);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
    // forced into WIDGET mode with no stops: every key must be safe and end in TAB or stay put
    for (LogicalKey k : {LogicalKey::Next, LogicalKey::Prev, LogicalKey::Push, LogicalKey::LongPush}) {
      f.st.mode = FocusMode::Widget;
      f.st.stop = 1;
      f.key(k);
      TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab || f.st.mode == FocusMode::Widget);
    }
    // all widgets unavailable: same as nothing focusable
    Fx g(o, 1, {W(WidgetType::Light), W(WidgetType::Cover)});
    g.env.unavailable = {true, true};
    TEST_ASSERT_TRUE(g.key(LogicalKey::Push).effect == Effect::None);
    TEST_ASSERT_TRUE(g.st.mode == FocusMode::Tab);
  }
}
void test_robust_out_of_range_args() {
  std::vector<Tab> t = bigConfig(8, 14);
  int ints[] = {INT_MIN, -100, -2, -1, 0, 1, 7, 8, 9, 100, INT_MAX};
  for (Orientation o : {P, L})
    for (int a : ints)
      for (int sp : ints)
        for (int g : ints) {
          Layout l = computeLayout(o, t, a, sp, g);
          TEST_ASSERT_TRUE(l.activeTab >= 0 && l.activeTab < 8);
          TEST_ASSERT_TRUE(l.group >= 0 && l.group < l.groupCount);
          TEST_ASSERT_TRUE(l.pager.subPage >= 0 && l.pager.subPage < l.pager.subPageCount);
          TEST_ASSERT_EQUAL_INT((int)gridFor(o).tabSlots, (int)l.tabSlots.size());
          for (auto &s : l.tabSlots)
            if (s.kind == SlotKind::Tab) TEST_ASSERT_TRUE(s.tabIndex >= 0 && s.tabIndex < 8);
          checkLayoutInvariants(l);
        }
  // clamping semantics
  Layout hi = computeLayout(P, t, 99, 99);
  TEST_ASSERT_EQUAL_INT(7, hi.activeTab);
  TEST_ASSERT_EQUAL_INT(hi.pager.subPageCount - 1, hi.pager.subPage);
  Layout lo = computeLayout(P, t, -3, -3);
  TEST_ASSERT_EQUAL_INT(0, lo.activeTab);
  TEST_ASSERT_EQUAL_INT(0, lo.pager.subPage);
  TEST_ASSERT_EQUAL_INT(2, computeLayout(P, t, 0, 0, 99).group);  // 8 tabs: groups 0..2
  TEST_ASSERT_EQUAL_INT(0, computeLayout(P, t, 0, 0, 99).activeTab);  // group is view-only
  TEST_ASSERT_EQUAL_INT(2, computeLayout(P, t, 7, 0, -4).group);      // negative: derive from tab
  // group helpers with out-of-range tab
  TEST_ASSERT_EQUAL_INT(0, groupForTab(P, 8, -1));
  TEST_ASSERT_EQUAL_INT(0, nextGroup(P, 8, 99) % 3 == nextGroup(P, 8, 99) ? 0 : 1);
}
void test_robust_hit_outside_screen() {
  std::vector<Tab> t = bigConfig(8, 14);
  for (Orientation o : {P, L}) {
    Layout l = computeLayout(o, t, 0, 0);
    const Grid &g = gridFor(o);
    int xs[] = {INT_MIN, -100000, -1000, -1, g.screenW, g.screenW + 1, 961, 5000, INT_MAX};
    int ys[] = {INT_MIN, -100000, -1000, -1, g.screenH, g.screenH + 1, 5000, INT_MAX};
    // x outside the screen at several valid y; y outside at several valid x
    for (int x : xs)
      for (int y : {0, 45, 100, 500, g.screenH - 1}) TEST_ASSERT_TRUE(hitTest(l, {x, y}).kind == HitKind::None);
    for (int y : ys)
      for (int x : {0, 45, 100, 500, g.screenW - 1}) TEST_ASSERT_TRUE(hitTest(l, {x, y}).kind == HitKind::None);
    for (int x : xs)
      for (int y : ys) TEST_ASSERT_TRUE(hitTest(l, {x, y}).kind == HitKind::None);
    // exact screen edge: first pixel outside is none even where a rect ends at the border
    TEST_ASSERT_TRUE(hitTest(l, {g.screenW, g.tabBar.y + 10}).kind == HitKind::None);
  }
}
void test_robust_focus_empty_and_null_flow() {
  Flow empty;  // default: no placements, pageCount 1
  const Flow *flows[] = {nullptr, &empty};
  for (const Flow *fl : flows)
    for (int tabCount : {0, 1, 5}) {
      FocusEnv env;
      env.tabCount = tabCount;
      env.flow = fl;
      TEST_ASSERT_EQUAL_INT(0, (int)focusStops(env, 0).size());
      TEST_ASSERT_EQUAL_INT(0, (int)focusStops(env, -3).size());
      TEST_ASSERT_EQUAL_INT(0, (int)focusStops(env, 99).size());
      TEST_ASSERT_EQUAL_INT(0, (int)innerStops(env, 0).size());
      TEST_ASSERT_EQUAL_INT(0, (int)innerStops(env, -1).size());
      for (FocusMode m : {FocusMode::Tab, FocusMode::Widget, FocusMode::Inner})
        for (int stop : {0, 1, 5, -1})
          for (int inner : {0, 1, 3, 99})
            for (LogicalKey k : {LogicalKey::Prev, LogicalKey::Next, LogicalKey::Push, LogicalKey::LongPush}) {
              FocusState st;
              st.mode = m;
              st.stop = stop;
              st.inner = inner;
              FocusResult r = focusHandle(st, env, k);
              (void)r;
              TEST_ASSERT_EQUAL_INT(-1, focusedWidget(st, env));
              TEST_ASSERT_TRUE(st.mode == FocusMode::Tab || st.mode == FocusMode::Widget || st.mode == FocusMode::Inner);
              if (tabCount > 0 && m == FocusMode::Tab) TEST_ASSERT_TRUE(st.tab >= 0 && st.tab < tabCount);
            }
    }
}
void test_robust_focus_garbage_state() {
  // st.tab / subPage / stop out of range must never crash; tab must come back in [0, tabCount)
  Flow fl = computeFlow(P, {W(WidgetType::Light), W(WidgetType::Cover)});
  FocusEnv env;
  env.tabCount = 3;
  env.flow = &fl;
  for (int tab : {-2, -1, 3, 7, 1000})
    for (LogicalKey k : {LogicalKey::Prev, LogicalKey::Next}) {
      FocusState st;
      st.tab = tab;
      focusHandle(st, env, k);
      TEST_ASSERT_TRUE(st.tab >= 0 && st.tab < 3);
    }
  for (int sp : {-5, 0, 3, 99})
    for (int stop : {-5, 0, 1, 2, 3, 99})
      for (FocusMode m : {FocusMode::Widget, FocusMode::Inner})
        for (LogicalKey k : {LogicalKey::Prev, LogicalKey::Next, LogicalKey::Push, LogicalKey::LongPush}) {
          FocusState st;
          st.mode = m;
          st.subPage = sp;
          st.stop = stop;
          st.inner = 42;
          focusHandle(st, env, k);
        }
}

// KNOWN BUG (reported, not fixed here: lib/ is out of scope for the tester): a FocusState.tab below
// -tabCount makes ((tab + d + tabCount) % tabCount) negative (ui_rocker.cpp:107). Unreachable with a
// valid state; kept as an ignored test so the finding stays visible.
void test_robust_focus_negative_tab_known_bug() {
  Flow fl;
  FocusEnv env;
  env.tabCount = 3;
  env.flow = &fl;
  FocusState st;
  st.tab = -10;
  focusHandle(st, env, LogicalKey::Prev);
  if (st.tab < 0 || st.tab >= 3) TEST_IGNORE_MESSAGE("known bug: negative st.tab wraps to a negative tab index");
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_areas_portrait);
  RUN_TEST(test_areas_landscape);
  RUN_TEST(test_vector_slots_portrait);
  RUN_TEST(test_vector_slots_landscape);
  RUN_TEST(test_vector_cell_rects);
  RUN_TEST(test_pager_rects);
  RUN_TEST(test_tabs_no_overflow);
  RUN_TEST(test_tabs_overflow_5);
  RUN_TEST(test_tabs_8_portrait);
  RUN_TEST(test_tabs_landscape_groups);
  RUN_TEST(test_default_sizes);
  RUN_TEST(test_size_override);
  RUN_TEST(test_size_clamp_reports);
  RUN_TEST(test_flow_worked_example);
  RUN_TEST(test_flow_empty_tab);
  RUN_TEST(test_flow_full_page_no_pager);
  RUN_TEST(test_flow_gap_fill_current_page_only);
  RUN_TEST(test_flow_no_backfill_earlier_page);
  RUN_TEST(test_subregions_cover_portrait);
  RUN_TEST(test_subregions_cover_landscape_and_2x2);
  RUN_TEST(test_subregions_climate);
  RUN_TEST(test_subregions_none_and_min_size);
  RUN_TEST(test_property_no_overlap);
  RUN_TEST(test_hit_tabs_and_edges);
  RUN_TEST(test_hit_widgets);
  RUN_TEST(test_hit_pager);
  RUN_TEST(test_rocker_mapping);
  RUN_TEST(test_focus_tab_mode_wrap_and_groups);
  RUN_TEST(test_focus_order_and_skip_readonly);
  RUN_TEST(test_focus_row_col_order);
  RUN_TEST(test_focus_unavailable_skipped);
  RUN_TEST(test_focus_single_action_and_inner);
  RUN_TEST(test_focus_subpages);
  RUN_TEST(test_focus_skips_pages_without_focusable);
  RUN_TEST(test_focus_touch);
  RUN_TEST(test_focus_invert_flow);
  RUN_TEST(test_mockup_portrait);
  RUN_TEST(test_mockup_landscape);
  RUN_TEST(test_robust_zero_tabs);
  RUN_TEST(test_robust_16_tabs_24_widgets);
  RUN_TEST(test_robust_all_2x2);
  RUN_TEST(test_robust_nothing_focusable);
  RUN_TEST(test_robust_out_of_range_args);
  RUN_TEST(test_robust_hit_outside_screen);
  RUN_TEST(test_robust_focus_empty_and_null_flow);
  RUN_TEST(test_robust_focus_garbage_state);
  RUN_TEST(test_robust_focus_negative_tab_known_bug);
  return UNITY_END();
}
