#include <climits>
#include <cstdio>
#include <fstream>
#include <sstream>
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

// ---- geometry (layout.md section 2 and 8.1) ----
void test_areas_portrait() {
  Layout l = computeLayout(P, tabsOf(1), 0, 0);
  eqRect({0, 0, 540, 56}, l.statusBar);
  eqRect({0, 880, 540, 80}, l.tabBar);
  eqRect({16, 68, 508, 800}, l.content);
  eqRect({0, 0, 288, 56}, l.refreshHit);
  eqRect({412, 0, 112, 56}, l.settingsHit);
  eqRect({292, 3, 112, 48}, l.sleepIndicator);
  eqRect({292, 3, 112, 48}, sleepIndicatorRect(P));
  const Grid &g = gridFor(P);
  TEST_ASSERT_EQUAL_INT(2, g.cols);
  TEST_ASSERT_EQUAL_INT(7, g.rows);
  TEST_ASSERT_EQUAL_INT(248, g.cellW);
  int ry[] = {68, 184, 300, 416, 532, 648, 764};
  for (int i = 0; i < 7; ++i) TEST_ASSERT_EQUAL_INT(ry[i], g.rowY[i]);
  TEST_ASSERT_EQUAL_INT(16, g.colX[0]);
  TEST_ASSERT_EQUAL_INT(276, g.colX[1]);
}
void test_areas_landscape() {
  Layout l = computeLayout(L, tabsOf(1), 0, 0);
  eqRect({0, 0, 960, 64}, l.statusBar);
  eqRect({172, 0, 648, 64}, l.tabBar);
  eqRect({16, 72, 928, 452}, l.content);
  eqRect({16, 4, 144, 56}, l.refreshHit);
  eqRect({832, 4, 112, 56}, l.settingsHit);
  eqRect({172, 4, 648, 56}, l.sleepIndicator);
  eqRect({172, 4, 648, 56}, sleepIndicatorRect(L));
  const Grid &g = gridFor(L);
  TEST_ASSERT_EQUAL_INT(5, g.cols);
  TEST_ASSERT_EQUAL_INT(4, g.rows);
  TEST_ASSERT_EQUAL_INT(176, g.cellW);
  int cx[] = {16, 204, 392, 580, 768};
  for (int i = 0; i < 5; ++i) TEST_ASSERT_EQUAL_INT(cx[i], g.colX[i]);
  int ry[] = {72, 188, 304, 420};
  for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_INT(ry[i], g.rowY[i]);
}
void test_closing_sums() {
  const Grid &p = gridFor(P);
  const Grid &l = gridFor(L);
  TEST_ASSERT_EQUAL_INT(540, 16 + p.cellW + kGap + p.cellW + 16);
  TEST_ASSERT_EQUAL_INT(960, p.statusBar.h + kGap + (p.rows * kCellH + (p.rows - 1) * kGap) + kGap + p.tabBar.h);
  TEST_ASSERT_EQUAL_INT(960, l.screenW);
  TEST_ASSERT_EQUAL_INT(960, 16 + (l.cols * l.cellW + (l.cols - 1) * kGap) + 16);
  TEST_ASSERT_EQUAL_INT(540, l.content.y + (l.rows * kCellH + (l.rows - 1) * kGap) + 16);
  TEST_ASSERT_EQUAL_INT(l.content.y, l.slotRect[0].y + l.slotRect[0].h + kGap);  // 60 + 12 = 72
  TEST_ASSERT_EQUAL_INT(960, 16 + 144 + 12 + 648 + 12 + 112 + 16);
  // content areas are exactly the grid extents
  eqRect(p.content, cellRect(p, 0, 0, 2, 7));
  eqRect(l.content, cellRect(l, 0, 0, 5, 4));
  // status / hit rects never overlap tab slots, each other or the grid
  for (Orientation o : {P, L}) {
    const Grid &g = gridFor(o);
    TEST_ASSERT_FALSE(rectsOverlap(g.refreshHit, g.settingsHit));
    TEST_ASSERT_FALSE(rectsOverlap(g.refreshHit, g.content));
    TEST_ASSERT_FALSE(rectsOverlap(g.settingsHit, g.content));
    TEST_ASSERT_TRUE(rectInside(g.sleepIndicator, g.statusBar));
    TEST_ASSERT_FALSE(rectsOverlap(g.sleepIndicator, g.refreshHit));
    TEST_ASSERT_FALSE(rectsOverlap(g.sleepIndicator, g.settingsHit));
    for (int i = 0; i < g.tabSlots; ++i) {
      TEST_ASSERT_FALSE(rectsOverlap(g.slotRect[i], g.refreshHit));
      TEST_ASSERT_FALSE(rectsOverlap(g.slotRect[i], g.settingsHit));
      TEST_ASSERT_FALSE(rectsOverlap(g.slotRect[i], g.content));
      TEST_ASSERT_TRUE(rectInside(g.slotRect[i], g.tabBar));
    }
  }
  // portrait indicator sits between the refresh hit area and the settings hit area
  TEST_ASSERT_TRUE(p.sleepIndicator.x >= p.refreshHit.x + p.refreshHit.w);
  TEST_ASSERT_TRUE(p.sleepIndicator.x + p.sleepIndicator.w <= p.settingsHit.x);
}
void test_vector_slots_portrait() {
  Layout l = computeLayout(P, tabsOf(4), 0, 0);
  TEST_ASSERT_EQUAL_INT(4, (int)l.tabSlots.size());
  Rect e[] = {{16, 892, 118, 56}, {146, 892, 118, 56}, {276, 892, 118, 56}, {406, 892, 118, 56}};
  for (int i = 0; i < 4; ++i) eqRect(e[i], l.tabSlots[i].rect);
}
void test_vector_slots_landscape() {
  Layout l = computeLayout(L, tabsOf(5), 0, 0);
  TEST_ASSERT_EQUAL_INT(5, (int)l.tabSlots.size());
  Rect e[] = {{172, 4, 120, 56}, {304, 4, 120, 56}, {436, 4, 120, 56}, {568, 4, 120, 56}, {700, 4, 120, 56}};
  for (int i = 0; i < 5; ++i) eqRect(e[i], l.tabSlots[i].rect);
}
void test_vector_cell_rects() {
  eqRect({276, 68, 248, 104}, cellRect(gridFor(P), 1, 0, 1, 1));
  eqRect({16, 184, 508, 104}, cellRect(gridFor(P), 0, 1, 2, 1));
  // synthetic multi-row geometry (no real type is taller than one row)
  eqRect({16, 300, 248, 220}, cellRect(gridFor(P), 0, 2, 1, 2));
  eqRect({16, 532, 508, 336}, cellRect(gridFor(P), 0, 4, 2, 3));  // bottom edge 868
  eqRect({16, 72, 176, 452}, cellRect(gridFor(L), 0, 0, 1, 4));   // bottom edge 524
  eqRect({16, 72, 552, 104}, cellRect(gridFor(L), 0, 0, 3, 1));
  eqRect({768, 72, 176, 104}, cellRect(gridFor(L), 4, 0, 1, 1));
  eqRect({392, 420, 364, 104}, cellRect(gridFor(L), 2, 3, 2, 1));
  eqRect({204, 304, 740, 104}, cellRect(gridFor(L), 1, 2, 4, 1));  // right edge 944
  eqRect({16, 72, 928, 104}, cellRect(gridFor(L), 0, 0, 5, 1));
}
void test_pager_rects() {
  std::vector<Tab> t = tabsOf(1);
  for (int i = 0; i < 8; ++i) t[0].widgets.push_back(W(WidgetType::Climate));
  Layout p = computeLayout(P, t, 0, 0);
  TEST_ASSERT_TRUE(p.pager.visible);
  eqRect({16, 764, 508, 104}, p.pager.rect);
  eqRect({16, 784, 120, 64}, p.pager.prev);
  eqRect({404, 784, 120, 64}, p.pager.next);
  eqRect({148, 784, 244, 64}, p.pager.label);
  for (int i = 0; i < 21; ++i) t[0].widgets.push_back(W(WidgetType::Light));
  Layout l = computeLayout(L, t, 0, 0);
  TEST_ASSERT_TRUE(l.pager.visible);
  eqRect({16, 420, 928, 104}, l.pager.rect);
  eqRect({16, 440, 120, 64}, l.pager.prev);
  eqRect({824, 440, 120, 64}, l.pager.next);
  eqRect({148, 440, 664, 64}, l.pager.label);
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
  TEST_ASSERT_EQUAL_INT(6, groupCount(P, 16));
}
void test_tabs_landscape_groups() {
  TEST_ASSERT_EQUAL_INT(4, tabsPerGroup(L, 16));
  TEST_ASSERT_EQUAL_INT(4, groupCount(L, 16));
  TEST_ASSERT_EQUAL_INT(1, groupCount(L, 5));
  TEST_ASSERT_EQUAL_INT(2, groupCount(L, 6));
  Layout five = computeLayout(L, tabsOf(5), 0, 0);
  for (int i = 0; i < 5; ++i) TEST_ASSERT_TRUE(five.tabSlots[i].kind == SlotKind::Tab);
  Layout l = computeLayout(L, tabsOf(6), 5, 0);
  TEST_ASSERT_EQUAL_INT(1, l.group);
  TEST_ASSERT_EQUAL_INT(4, l.tabSlots[0].tabIndex);
  TEST_ASSERT_EQUAL_INT(5, l.tabSlots[1].tabIndex);
  TEST_ASSERT_TRUE(l.tabSlots[2].kind == SlotKind::Empty);
  TEST_ASSERT_TRUE(l.tabSlots[3].kind == SlotKind::Empty);
  TEST_ASSERT_TRUE(l.tabSlots[4].kind == SlotKind::Overflow);
}

// ---- sizes (layout.md section 3 and 8.2) ----
void test_default_sizes() {
  struct { WidgetType t; int pw, lw; } d[] = {
      {WidgetType::Cover, 2, 3}, {WidgetType::Light, 1, 1}, {WidgetType::Switch, 1, 1},
      {WidgetType::Automation, 1, 1}, {WidgetType::Climate, 2, 3}, {WidgetType::Sensor, 1, 1},
      {WidgetType::BinarySensor, 1, 1}, {WidgetType::Weather, 2, 2}, {WidgetType::Vacuum, 2, 3}};
  for (auto &e : d)
    for (Orientation o : {P, L}) {
      unsigned why = 99;
      WidgetSize s = resolveSize(e.t, W(e.t), gridFor(o), &why);
      TEST_ASSERT_EQUAL_INT(o == P ? e.pw : e.lw, s.w);
      TEST_ASSERT_EQUAL_INT(1, s.h);
      TEST_ASSERT_EQUAL_UINT(0, why);
    }
  // limits table: sensor max 2x1, weather min 1x1
  SizeLimits sl = sizeLimits(WidgetType::Sensor, P);
  TEST_ASSERT_EQUAL_INT(2, sl.max.w);
  SizeLimits wl = sizeLimits(WidgetType::Weather, L);
  TEST_ASSERT_EQUAL_INT(1, wl.min.w);
  TEST_ASSERT_EQUAL_INT(2, wl.max.w);
}
void test_size_override() {
  WidgetSize s = resolveSize(WidgetType::Sensor, W(WidgetType::Sensor, 2, 1), gridFor(L));
  TEST_ASSERT_EQUAL_INT(2, s.w);
  TEST_ASSERT_EQUAL_INT(1, s.h);
  s = resolveSize(WidgetType::Weather, W(WidgetType::Weather, 1, 1), gridFor(P));
  TEST_ASSERT_EQUAL_INT(1, s.w);
  TEST_ASSERT_EQUAL_INT(1, s.h);
}
static void expectClamp(WidgetType t, int rw, int rh, Orientation o, int uw, int uh, bool reported) {
  unsigned why = 0;
  WidgetSize req;
  WidgetSize s = resolveSize(t, W(t, rw, rh), gridFor(o), &why, &req);
  TEST_ASSERT_EQUAL_INT(rw, req.w);
  TEST_ASSERT_EQUAL_INT(rh, req.h);
  TEST_ASSERT_EQUAL_INT(uw, s.w);
  TEST_ASSERT_EQUAL_INT(uh, s.h);
  TEST_ASSERT_EQUAL(reported, why != 0);
  Flow f = computeFlow(o, {W(t, rw, rh)});
  TEST_ASSERT_EQUAL_INT(reported ? 1 : 0, (int)f.clampReports.size());
  if (reported) {
    TEST_ASSERT_EQUAL_INT(0, f.clampReports[0].widgetIndex);
    TEST_ASSERT_EQUAL_INT(rw, f.clampReports[0].requested.w);
    TEST_ASSERT_EQUAL_INT(rh, f.clampReports[0].requested.h);
    TEST_ASSERT_EQUAL_INT(uw, f.clampReports[0].used.w);
    TEST_ASSERT_EQUAL_INT(uh, f.clampReports[0].used.h);
    TEST_ASSERT_TRUE(f.placements[0].clamped);
  }
}
void test_size_clamp_reports() {
  expectClamp(WidgetType::Cover, 2, 1, L, 3, 1, true);
  expectClamp(WidgetType::Cover, 4, 2, L, 3, 1, true);
  expectClamp(WidgetType::Cover, 4, 1, P, 2, 1, true);
  expectClamp(WidgetType::Light, 2, 1, P, 1, 1, true);
  expectClamp(WidgetType::Sensor, 3, 1, P, 2, 1, true);
  expectClamp(WidgetType::Weather, 4, 1, P, 2, 1, true);
  expectClamp(WidgetType::Weather, 4, 1, L, 2, 1, true);
  expectClamp(WidgetType::Climate, 2, 2, P, 2, 1, true);
  expectClamp(WidgetType::Climate, 2, 2, L, 3, 1, true);
  expectClamp(WidgetType::BinarySensor, 2, 1, P, 1, 1, true);
  // a request equal to the default is not reported
  expectClamp(WidgetType::Cover, 2, 1, P, 2, 1, false);
  expectClamp(WidgetType::Cover, 3, 1, L, 3, 1, false);
  expectClamp(WidgetType::Weather, 2, 1, L, 2, 1, false);
  expectClamp(WidgetType::Light, 1, 1, L, 1, 1, false);
  // type max reason / type min reason
  unsigned why = 0;
  resolveSize(WidgetType::Light, W(WidgetType::Light, 3, 3), gridFor(P), &why);
  TEST_ASSERT_TRUE(why & kClampTypeMax);
  resolveSize(WidgetType::Climate, W(WidgetType::Climate, 1, 1), gridFor(P), &why);
  TEST_ASSERT_TRUE(why & kClampTypeMin);
}
void test_size_clamp_rows_synthetic() {
  // Step 3 (rows - 1) is a safeguard; only reachable with a synthetic limits table.
  SizeLimits tall = {{1, 1}, {1, 1}, {1, 9}};
  unsigned why = 0;
  WidgetSize s = clampSize(tall, {1, 7}, gridFor(P), &why);
  TEST_ASSERT_EQUAL_INT(6, s.h);
  TEST_ASSERT_TRUE(why & kClampRows);
  s = clampSize(tall, {1, 4}, gridFor(P), &why);
  TEST_ASSERT_EQUAL_INT(4, s.h);
  TEST_ASSERT_EQUAL_UINT(0, why);
  s = clampSize(tall, {1, 4}, gridFor(L), &why);
  TEST_ASSERT_EQUAL_INT(3, s.h);
  TEST_ASSERT_TRUE(why & kClampRows);
  s = clampSize(tall, {1, 3}, gridFor(L), &why);
  TEST_ASSERT_EQUAL_INT(3, s.h);
  TEST_ASSERT_EQUAL_UINT(0, why);
  // columns step
  SizeLimits wide = {{1, 1}, {1, 1}, {9, 1}};
  s = clampSize(wide, {9, 1}, gridFor(P), &why);
  TEST_ASSERT_EQUAL_INT(2, s.w);
  TEST_ASSERT_TRUE(why & kClampColumns);
}

// ---- flow (layout.md section 4 and 8.2) ----
// Worked example of layout.md section 4 (10 widgets).
static std::vector<Widget> workedExample() {
  Widget grp = W(WidgetType::Cover);
  grp.group = "g";
  return {W(WidgetType::Cover), grp, W(WidgetType::Light), W(WidgetType::Light), W(WidgetType::Switch),
          W(WidgetType::Climate), W(WidgetType::BinarySensor), W(WidgetType::Sensor),
          W(WidgetType::Automation), W(WidgetType::Weather)};
}
// Small fixture used by rocker/hit tests: cover, group, climate, sensor, cover.
static std::vector<Widget> livingRoom() {
  Widget grp = W(WidgetType::Cover);
  grp.group = "g";
  return {W(WidgetType::Light), grp, W(WidgetType::Climate), W(WidgetType::Sensor), W(WidgetType::Cover)};
}
void test_flow_worked_example() {
  Flow p = computeFlow(P, workedExample());
  TEST_ASSERT_EQUAL_INT(1, p.pageCount);
  TEST_ASSERT_FALSE(p.pagerVisible);
  TEST_ASSERT_EQUAL_INT(0, (int)p.clampReports.size());
  struct { int c, r; Rect rect; } pe[] = {
      {0, 0, {16, 68, 508, 104}},  {0, 1, {16, 184, 508, 104}}, {0, 2, {16, 300, 248, 104}},
      {1, 2, {276, 300, 248, 104}}, {0, 3, {16, 416, 248, 104}}, {0, 4, {16, 532, 508, 104}},
      {1, 3, {276, 416, 248, 104}}, {0, 5, {16, 648, 248, 104}}, {1, 5, {276, 648, 248, 104}},
      {0, 6, {16, 764, 508, 104}}};
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT_EQUAL_INT(pe[i].c, p.placements[i].cellX);
    TEST_ASSERT_EQUAL_INT(pe[i].r, p.placements[i].cellY);
    eqRect(pe[i].rect, p.placements[i].rect);
  }
  Flow l = computeFlow(L, workedExample());
  TEST_ASSERT_EQUAL_INT(1, l.pageCount);
  TEST_ASSERT_FALSE(l.pagerVisible);
  struct { int c, r; Rect rect; } le[] = {
      {0, 0, {16, 72, 552, 104}},  {0, 1, {16, 188, 552, 104}}, {3, 0, {580, 72, 176, 104}},
      {4, 0, {768, 72, 176, 104}}, {3, 1, {580, 188, 176, 104}}, {0, 2, {16, 304, 552, 104}},
      {4, 1, {768, 188, 176, 104}}, {3, 2, {580, 304, 176, 104}}, {4, 2, {768, 304, 176, 104}},
      {0, 3, {16, 420, 364, 104}}};
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT_EQUAL_INT(le[i].c, l.placements[i].cellX);
    TEST_ASSERT_EQUAL_INT(le[i].r, l.placements[i].cellY);
    eqRect(le[i].rect, l.placements[i].rect);
  }
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
void test_flow_capacity() {
  // portrait: 7 rows x 2 = 14 cells; the 15th 1x1 triggers the pager (6 rows = 12 cells on page 0)
  std::vector<Widget> w(14, W(WidgetType::Light));
  Flow f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(1, f.pageCount);
  TEST_ASSERT_FALSE(f.pagerVisible);
  w.push_back(W(WidgetType::Light));
  f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_TRUE(f.pagerVisible);
  TEST_ASSERT_EQUAL_INT(0, f.placements[11].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[12].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[14].page);
  TEST_ASSERT_EQUAL_INT(0, f.placements[12].cellX);
  TEST_ASSERT_EQUAL_INT(0, f.placements[12].cellY);
  // landscape: 4 x 5 = 20 cells; the 21st triggers the pager (3 rows = 15 cells on page 0)
  std::vector<Widget> v(20, W(WidgetType::Light));
  f = computeFlow(L, v);
  TEST_ASSERT_EQUAL_INT(1, f.pageCount);
  v.push_back(W(WidgetType::Light));
  f = computeFlow(L, v);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_TRUE(f.pagerVisible);
  TEST_ASSERT_EQUAL_INT(0, f.placements[14].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[15].page);
}
void test_flow_row_fill() {
  // landscape: 5 x 1x1 fill one row, a 6th starts row 1; portrait: 2 fill a row, a 3rd starts row 1
  std::vector<Widget> w(6, W(WidgetType::Light));
  Flow f = computeFlow(L, w);
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT_EQUAL_INT(i, f.placements[i].cellX);
    TEST_ASSERT_EQUAL_INT(0, f.placements[i].cellY);
  }
  TEST_ASSERT_EQUAL_INT(0, f.placements[5].cellX);
  TEST_ASSERT_EQUAL_INT(1, f.placements[5].cellY);
  Flow p = computeFlow(P, std::vector<Widget>(3, W(WidgetType::Light)));
  TEST_ASSERT_EQUAL_INT(1, p.placements[1].cellX);
  TEST_ASSERT_EQUAL_INT(0, p.placements[1].cellY);
  TEST_ASSERT_EQUAL_INT(0, p.placements[2].cellX);
  TEST_ASSERT_EQUAL_INT(1, p.placements[2].cellY);
}
void test_flow_overflow_portrait_wide() {
  std::vector<Widget> w(7, W(WidgetType::Cover));
  Flow f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(1, f.pageCount);  // 7 wide widgets fill the 7 rows
  TEST_ASSERT_FALSE(f.pagerVisible);
  w.push_back(W(WidgetType::Cover));
  f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_TRUE(f.pagerVisible);
  for (int i = 0; i < 6; ++i) {
    TEST_ASSERT_EQUAL_INT(0, f.placements[i].page);
    TEST_ASSERT_EQUAL_INT(i, f.placements[i].cellY);
  }
  TEST_ASSERT_EQUAL_INT(1, f.placements[6].page);
  TEST_ASSERT_EQUAL_INT(0, f.placements[6].cellY);
  TEST_ASSERT_EQUAL_INT(1, f.placements[7].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[7].cellY);
  Layout l = computeLayout(P, [&] { auto t = tabsOf(1); t[0].widgets = w; return t; }(), 0, 0);
  TEST_ASSERT_EQUAL_INT(2, l.pager.subPageCount);
  TEST_ASSERT_TRUE(l.pager.visible);
  TEST_ASSERT_EQUAL_INT(6, (int)l.widgets.size());
}
void test_flow_gap_fill_current_page_only() {
  // landscape pager mode (3 rows x 5 cols). Three cover+weather rows fill page 0. Page 1 gets
  // cover, cover, light: the light fills the gap beside the first cover on page 1, never page 0.
  std::vector<Widget> w;
  for (int i = 0; i < 3; ++i) {
    w.push_back(W(WidgetType::Cover));
    w.push_back(W(WidgetType::Weather));
  }
  w.push_back(W(WidgetType::Cover));
  w.push_back(W(WidgetType::Cover));
  w.push_back(W(WidgetType::Light));
  Flow f = computeFlow(L, w);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_EQUAL_INT(0, f.placements[5].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[6].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[7].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[7].cellY);
  TEST_ASSERT_EQUAL_INT(1, f.placements[8].page);
  TEST_ASSERT_EQUAL_INT(3, f.placements[8].cellX);
  TEST_ASSERT_EQUAL_INT(0, f.placements[8].cellY);
}
void test_flow_no_backfill_earlier_page() {
  // portrait pager mode: 6 wide per page. 6 covers fill page 0; the light goes to page 1 and the
  // following light fills the gap beside it, a later wide cover cannot return to page 0.
  std::vector<Widget> w(6, W(WidgetType::Cover));
  w.push_back(W(WidgetType::Light));  // page 1 (0,0)
  w.push_back(W(WidgetType::Cover));  // page 1 row 1
  w.push_back(W(WidgetType::Light));  // page 1 gap (1,0)
  Flow f = computeFlow(P, w);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);
  TEST_ASSERT_EQUAL_INT(1, f.placements[7].cellY);
  TEST_ASSERT_EQUAL_INT(1, f.placements[8].page);
  TEST_ASSERT_EQUAL_INT(1, f.placements[8].cellX);
  TEST_ASSERT_EQUAL_INT(0, f.placements[8].cellY);
  // three pages (6, 6, 1)
  std::vector<Widget> c(13, W(WidgetType::Climate));
  Flow f3 = computeFlow(P, c);
  TEST_ASSERT_EQUAL_INT(3, f3.pageCount);
  Layout l = computeLayout(P, [&] { auto t = tabsOf(1); t[0].widgets = c; return t; }(), 0, 7);
  TEST_ASSERT_EQUAL_INT(2, l.pager.subPage);
  TEST_ASSERT_EQUAL_INT(1, (int)l.widgets.size());
  TEST_ASSERT_EQUAL_INT(12, l.widgets[0].index);
}

// ---- sub-regions (layout.md section 6, widgets.md) ----
void test_subregions_cover() {
  Rect rp = {16, 68, 508, 104};
  auto s = subRegions(WidgetType::Cover, {2, 1}, rp);
  TEST_ASSERT_EQUAL_INT(3, (int)s.size());
  eqRect({296, 88, 64, 64}, s[0].rect);
  eqRect({372, 88, 64, 64}, s[1].rect);
  eqRect({448, 88, 64, 64}, s[2].rect);
  TEST_ASSERT_EQUAL_INT(1, s[0].id);
  TEST_ASSERT_EQUAL_INT(2, s[1].id);
  TEST_ASSERT_EQUAL_INT(3, s[2].id);
  Rect rl = {16, 72, 552, 104};
  auto t = subRegions(WidgetType::Cover, {3, 1}, rl);
  eqRect({340, 92, 64, 64}, t[0].rect);
  eqRect({416, 92, 64, 64}, t[1].rect);
  eqRect({492, 92, 64, 64}, t[2].rect);
  auto v = subRegions(WidgetType::Vacuum, {2, 1}, rp);  // vacuum uses the cover rects
  TEST_ASSERT_EQUAL_INT(3, (int)v.size());
  for (int i = 0; i < 3; ++i) eqRect(s[i].rect, v[i].rect);
}
void test_subregions_climate() {
  auto s = subRegions(WidgetType::Climate, {2, 1}, {16, 532, 508, 104});
  TEST_ASSERT_EQUAL_INT(2, (int)s.size());  // the mode is display-only: no third sub-region
  eqRect({248, 552, 64, 64}, s[0].rect);
  eqRect({448, 552, 64, 64}, s[1].rect);
  TEST_ASSERT_EQUAL_INT(1, s[0].id);
  TEST_ASSERT_EQUAL_INT(2, s[1].id);
  auto l = subRegions(WidgetType::Climate, {3, 1}, {16, 304, 552, 104});
  eqRect({292, 324, 64, 64}, l[0].rect);
  eqRect({492, 324, 64, 64}, l[1].rect);
}
void test_subregions_none_and_geometry() {
  for (WidgetType t : {WidgetType::Light, WidgetType::Switch, WidgetType::Automation, WidgetType::Sensor,
                       WidgetType::BinarySensor, WidgetType::Weather})
    TEST_ASSERT_EQUAL_INT(0, (int)subRegions(t, {1, 1}, {0, 0, 248, 104}).size());
  for (Orientation o : {P, L})
    for (WidgetType t : {WidgetType::Cover, WidgetType::Vacuum, WidgetType::Climate}) {
      WidgetSize s = resolveSize(t, W(t), gridFor(o));
      Rect r = cellRect(gridFor(o), 0, 0, s.w, s.h);
      auto subs = subRegions(t, s, r);
      TEST_ASSERT_TRUE(subs.size() >= 2);
      for (size_t i = 0; i < subs.size(); ++i) {
        TEST_ASSERT_EQUAL_INT(64, subs[i].rect.w);
        TEST_ASSERT_EQUAL_INT(64, subs[i].rect.h);
        TEST_ASSERT_TRUE(rectInside(subs[i].rect, r));
        for (size_t j = i + 1; j < subs.size(); ++j) {
          TEST_ASSERT_FALSE(rectsOverlap(subs[i].rect, subs[j].rect));
          TEST_ASSERT_FALSE(rectsOverlap(inflate(subs[i].rect, 5), subs[j].rect));  // >= 12 apart
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

// ---- hit test (layout.md section 6 and 8.4) ----
static Hit H(const Layout &l, int x, int y) { return hitTest(l, {x, y}); }
static void expectHit(const Layout &l, int x, int y, HitKind k, int index = 0, int sub = 0) {
  Hit h = H(l, x, y);
  char msg[64];
  snprintf(msg, sizeof msg, "at (%d,%d) got kind %d index %d sub %d", x, y, (int)h.kind, h.index, h.sub);
  TEST_ASSERT_TRUE_MESSAGE(h.kind == k && h.index == index && h.sub == sub, msg);
}
void test_hit_status_portrait() {
  Layout l = computeLayout(P, tabsOf(4), 0, 0);
  expectHit(l, 0, 0, HitKind::Refresh);
  expectHit(l, 287, 55, HitKind::Refresh);
  expectHit(l, 288, 30, HitKind::None);
  expectHit(l, 300, 30, HitKind::None);  // sleep indicator region is not a target
  expectHit(l, 411, 30, HitKind::None);
  expectHit(l, 412, 0, HitKind::Settings);
  expectHit(l, 523, 55, HitKind::Settings);
  expectHit(l, 524, 30, HitKind::None);
  expectHit(l, 412, 56, HitKind::None);
  expectHit(l, 100, 56, HitKind::None);
  expectHit(l, 100, 67, HitKind::None);
}
void test_hit_status_landscape() {
  Layout l = computeLayout(L, tabsOf(5), 0, 0);
  expectHit(l, 16, 4, HitKind::Refresh);
  expectHit(l, 159, 59, HitKind::Refresh);
  expectHit(l, 160, 30, HitKind::None);
  expectHit(l, 171, 30, HitKind::None);
  expectHit(l, 172, 4, HitKind::Tab, 0);
  expectHit(l, 291, 59, HitKind::Tab, 0);
  expectHit(l, 292, 30, HitKind::None);
  expectHit(l, 304, 4, HitKind::Tab, 1);
  expectHit(l, 172, 60, HitKind::None);
  expectHit(l, 819, 30, HitKind::Tab, 4);
  expectHit(l, 820, 30, HitKind::None);
  expectHit(l, 831, 30, HitKind::None);
  expectHit(l, 832, 4, HitKind::Settings);
  expectHit(l, 943, 59, HitKind::Settings);
  expectHit(l, 944, 30, HitKind::None);
  expectHit(l, 15, 4, HitKind::None);
  expectHit(l, 16, 3, HitKind::None);
  expectHit(l, 16, 60, HitKind::None);
  expectHit(l, 16, 71, HitKind::None);  // 12 px strip above the grid
}
void test_hit_tabs_and_edges() {
  Layout l = computeLayout(P, tabsOf(5), 0, 0);
  expectHit(l, 16, 892, HitKind::Tab, 0);
  expectHit(l, 133, 892, HitKind::Tab, 0);
  expectHit(l, 134, 892, HitKind::None);
  expectHit(l, 146, 892, HitKind::Tab, 1);
  expectHit(l, 16, 891, HitKind::None);
  expectHit(l, 16, 948, HitKind::None);
  expectHit(l, 15, 892, HitKind::None);
  expectHit(l, 523, 947, HitKind::TabOverflow);
  Layout e = computeLayout(P, tabsOf(2), 0, 0);
  expectHit(e, 300, 900, HitKind::None);  // empty slot
  Layout e4 = computeLayout(P, tabsOf(4), 0, 0);
  expectHit(e4, 523, 947, HitKind::Tab, 3);
  Layout ls = computeLayout(L, tabsOf(3), 0, 0);
  expectHit(ls, 436, 10, HitKind::Tab, 2);
  expectHit(ls, 568, 10, HitKind::None);  // empty slot
}
void test_hit_widget_gaps() {
  std::vector<Tab> t = tabsOf(1);
  t[0].widgets = {W(WidgetType::Light), W(WidgetType::Light), W(WidgetType::Light)};
  Layout l = computeLayout(P, t, 0, 0);
  expectHit(l, 16, 68, HitKind::Widget, 0, 0);
  expectHit(l, 263, 100, HitKind::Widget, 0, 0);
  expectHit(l, 264, 100, HitKind::None);  // gap column x = 264..275
  expectHit(l, 275, 100, HitKind::None);
  expectHit(l, 276, 100, HitKind::Widget, 1, 0);
  expectHit(l, 300, 100, HitKind::Widget, 1, 0);
  expectHit(l, 16, 171, HitKind::Widget, 0, 0);
  expectHit(l, 16, 172, HitKind::None);  // row gap y = 172..183
  expectHit(l, 16, 183, HitKind::None);
  expectHit(l, 16, 184, HitKind::Widget, 2, 0);
  expectHit(l, 15, 68, HitKind::None);
  Layout ll = computeLayout(L, t, 0, 0);
  expectHit(ll, 16, 72, HitKind::Widget, 0, 0);
  expectHit(ll, 191, 100, HitKind::Widget, 0, 0);
  expectHit(ll, 192, 100, HitKind::None);
  expectHit(ll, 204, 100, HitKind::Widget, 1, 0);
}
void test_hit_subregions() {
  // cover at portrait cell (0,0): (16,68,508,104)
  std::vector<Tab> t = tabsOf(1);
  t[0].widgets = {W(WidgetType::Cover)};
  Layout l = computeLayout(P, t, 0, 0);
  expectHit(l, 296, 88, HitKind::Widget, 0, 1);
  expectHit(l, 359, 151, HitKind::Widget, 0, 1);
  expectHit(l, 360, 100, HitKind::Widget, 0, 0);  // 12 px gap between buttons
  expectHit(l, 372, 88, HitKind::Widget, 0, 2);
  expectHit(l, 448, 88, HitKind::Widget, 0, 3);
  expectHit(l, 511, 151, HitKind::Widget, 0, 3);
  expectHit(l, 512, 100, HitKind::Widget, 0, 0);  // padding
  expectHit(l, 296, 87, HitKind::Widget, 0, 0);   // above the buttons
  expectHit(l, 296, 152, HitKind::Widget, 0, 0);  // below the buttons
  // climate at portrait (0,0) and landscape (0,2)
  t[0].widgets = {W(WidgetType::Climate)};
  Layout c = computeLayout(P, t, 0, 0);
  expectHit(c, 248, 88, HitKind::Widget, 0, 1);
  expectHit(c, 311, 151, HitKind::Widget, 0, 1);
  expectHit(c, 448, 88, HitKind::Widget, 0, 2);
  expectHit(c, 380, 120, HitKind::Widget, 0, 0);  // value area
  // vacuum landscape
  t[0].widgets = {W(WidgetType::Vacuum)};
  Layout v = computeLayout(L, t, 0, 0);
  expectHit(v, 340, 92, HitKind::Widget, 0, 1);
  expectHit(v, 416, 92, HitKind::Widget, 0, 2);
  expectHit(v, 492, 92, HitKind::Widget, 0, 3);
  // toggles have no sub-regions: the whole rect is id 0
  t[0].widgets = {W(WidgetType::Switch)};
  Layout s = computeLayout(P, t, 0, 0);
  expectHit(s, 150, 120, HitKind::Widget, 0, 0);
}
void test_hit_pager() {
  std::vector<Tab> t = tabsOf(1);
  for (int i = 0; i < 15; ++i) t[0].widgets.push_back(W(WidgetType::Light));
  Layout l = computeLayout(P, t, 0, 0);
  TEST_ASSERT_TRUE(l.pager.visible);
  expectHit(l, 16, 784, HitKind::PagerPrev);
  expectHit(l, 135, 847, HitKind::PagerPrev);
  expectHit(l, 136, 784, HitKind::None);
  expectHit(l, 404, 784, HitKind::PagerNext);
  expectHit(l, 523, 847, HitKind::PagerNext);
  expectHit(l, 404, 783, HitKind::None);
  expectHit(l, 20, 751, HitKind::Widget, 10, 0);  // last widget row ends at 752 (rows 0..5)
  expectHit(l, 20, 764, HitKind::None);
  for (int i = 0; i < 6; ++i) t[0].widgets.push_back(W(WidgetType::Light));
  Layout l2 = computeLayout(L, t, 0, 0);
  TEST_ASSERT_TRUE(l2.pager.visible);
  expectHit(l2, 16, 440, HitKind::PagerPrev);
  expectHit(l2, 943, 503, HitKind::PagerNext);
}
void test_hit_dirty_inflation() {
  eqRect({10, 62, 520, 116}, inflate({16, 68, 508, 104}, 6));
  eqRect({270, 62, 260, 116}, inflate({276, 68, 248, 104}, 6));
  Rect a = inflate({16, 68, 248, 104}, 6), b = inflate({276, 68, 248, 104}, 6);
  TEST_ASSERT_EQUAL_INT(270, a.x + a.w);
  TEST_ASSERT_EQUAL_INT(270, b.x);
  TEST_ASSERT_FALSE(rectsOverlap(a, b));
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
    TEST_ASSERT_TRUE(f.key(LogicalKey::LongPush).effect == Effect::OpenSettings);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
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
void test_focus_history_sensors() {
  for (Orientation o : {P, L}) {
    Widget hs = W(WidgetType::Sensor);
    hs.entity = "sensor.t";
    Widget hb = W(WidgetType::BinarySensor);
    hb.entity = "binary_sensor.d";
    Widget off = hs;
    off.history.enabled = false;
    Widget attr = hs;
    attr.attribute = "battery";
    Widget noEntity = W(WidgetType::Sensor);
    Fx f(o, 2, {hs, W(WidgetType::Light), off, hb, attr, noEntity});
    auto stops = focusStops(f.env, 0);
    TEST_ASSERT_EQUAL_INT(3, (int)stops.size());  // hs, light, hb; the others stay unfocusable
    for (int w : stops) TEST_ASSERT_TRUE(w == 0 || w == 1 || w == 3);
    // without history flags the old behaviour holds
    Fx g(o, 2, {W(WidgetType::Sensor), W(WidgetType::Light)});
    TEST_ASSERT_EQUAL_INT(1, (int)focusStops(g.env, 0).size());
    TEST_ASSERT_FALSE(isFocusable(WidgetType::Sensor));
    TEST_ASSERT_TRUE(isFocusable(WidgetType::Sensor, true));
    TEST_ASSERT_FALSE(isFocusable(WidgetType::Weather, true));
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
  f.key(LogicalKey::Next);  // the mode is display-only: wraps to minus
  TEST_ASSERT_EQUAL_INT(1, f.st.inner);
  f.key(LogicalKey::Prev);  // wraps back to plus
  TEST_ASSERT_EQUAL_INT(2, f.st.inner);
  f.key(LogicalKey::LongPush);
  f.key(LogicalKey::Next);  // vacuum
  f.key(LogicalKey::Push);
  TEST_ASSERT_EQUAL_INT(1, f.st.inner);  // start
  f.key(LogicalKey::LongPush);
  f.key(LogicalKey::LongPush);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
}
void test_focus_subpages() {
  // portrait pager mode: 6 wide per page; 13 climates -> pages 6 / 6 / 1
  std::vector<Widget> w(13, W(WidgetType::Climate));
  Fx f(P, 1, w);
  TEST_ASSERT_EQUAL_INT(3, f.flow.pageCount);
  f.key(LogicalKey::Push);
  for (int i = 0; i < 5; ++i) f.key(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(5, focusedWidget(f.st, f.env));
  FocusResult r = f.key(LogicalKey::Next);
  TEST_ASSERT_TRUE(r.effect == Effect::SubPageChanged);
  TEST_ASSERT_EQUAL_INT(1, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(6, focusedWidget(f.st, f.env));
  // PREV at first stop of page 1 -> back to previous page, last stop
  r = f.key(LogicalKey::Prev);
  TEST_ASSERT_TRUE(r.effect == Effect::SubPageChanged);
  TEST_ASSERT_EQUAL_INT(0, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(5, focusedWidget(f.st, f.env));
  // go to the last page, NEXT after the last widget -> TAB mode, sub-page kept (2 stays 2)
  for (int i = 0; i < 7; ++i) f.key(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(2, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(12, focusedWidget(f.st, f.env));
  r = f.key(LogicalKey::Next);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  TEST_ASSERT_TRUE(r.effect == Effect::FocusMoved);
  TEST_ASSERT_TRUE(f.st.ringVisible);
  TEST_ASSERT_EQUAL_INT(2, f.st.subPage);
  // PUSH in TAB mode re-enters WIDGET mode on the first focusable widget of the kept sub-page
  r = f.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
  TEST_ASSERT_EQUAL_INT(2, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(12, focusedWidget(f.st, f.env));
  // PREV on the first widget of the first page also ends in TAB mode, page kept
  f.st.subPage = 0;
  f.st.stop = 1;
  f.key(LogicalKey::Prev);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  TEST_ASSERT_EQUAL_INT(0, f.st.subPage);
}
// 3 sub-pages where only sub-page 0 has a focusable widget: NEXT on its last widget is the end.
static std::vector<Widget> pagesOnlyFirstFocusable() {
  std::vector<Widget> w;
  w.push_back(W(WidgetType::Light));
  for (int i = 0; i < 16; ++i) w.push_back(W(WidgetType::Sensor));  // page 0: 12 cells (6 rows)
  for (int i = 0; i < 10; ++i) w.push_back(W(WidgetType::Sensor));  // rest on pages 1..2
  return w;
}
void test_focus_end_when_no_later_focusable_page() {
  Fx f(P, 1, pagesOnlyFirstFocusable());
  TEST_ASSERT_EQUAL_INT(3, f.flow.pageCount);
  TEST_ASSERT_EQUAL_INT(1, (int)focusStops(f.env, 0).size());
  f.key(LogicalKey::Push);
  FocusResult r = f.key(LogicalKey::Next);
  TEST_ASSERT_TRUE(r.effect == Effect::FocusMoved);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  TEST_ASSERT_EQUAL_INT(0, f.st.subPage);
}
void test_focus_end_when_no_earlier_focusable_page() {
  // only the last of 3 sub-pages has a focusable widget: PREV on its first widget is the end
  std::vector<Widget> w(26, W(WidgetType::Sensor));
  w.push_back(W(WidgetType::Light));
  Fx f(P, 1, w);
  TEST_ASSERT_EQUAL_INT(3, f.flow.pageCount);
  f.st.subPage = 2;
  TEST_ASSERT_EQUAL_INT(1, (int)focusStops(f.env, 2).size());
  f.st.mode = FocusMode::Tab;
  f.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
  f.key(LogicalKey::Prev);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  TEST_ASSERT_EQUAL_INT(2, f.st.subPage);
}
void test_focus_skips_pages_without_focusable() {
  // landscape pager mode (3 rows x 5): page 0 = 3 climates + 3 weathers, page 1 = 15 sensors
  // (nothing focusable), page 2 = a light
  std::vector<Widget> w;
  for (int i = 0; i < 3; ++i) w.push_back(W(WidgetType::Climate));
  for (int i = 0; i < 3; ++i) w.push_back(W(WidgetType::Weather));
  for (int i = 0; i < 15; ++i) w.push_back(W(WidgetType::Sensor));
  w.push_back(W(WidgetType::Light));
  Fx f(L, 1, w);
  TEST_ASSERT_EQUAL_INT(3, f.flow.pageCount);
  TEST_ASSERT_EQUAL_INT(0, (int)focusStops(f.env, 1).size());
  f.key(LogicalKey::Push);
  f.key(LogicalKey::Next);
  f.key(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(2, focusedWidget(f.st, f.env));
  FocusResult r = f.key(LogicalKey::Next);  // skip page 1
  TEST_ASSERT_TRUE(r.effect == Effect::SubPageChanged);
  TEST_ASSERT_EQUAL_INT(2, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(21, focusedWidget(f.st, f.env));
  r = f.key(LogicalKey::Prev);
  TEST_ASSERT_EQUAL_INT(0, f.st.subPage);
  TEST_ASSERT_EQUAL_INT(2, focusedWidget(f.st, f.env));
  f.key(LogicalKey::Prev);
  f.key(LogicalKey::Prev);
  r = f.key(LogicalKey::Prev);  // sub-page 0 first stop -> TAB
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
}
void test_focus_first_event_after_wake() {
  Fx f(P, 3, {W(WidgetType::Light)});
  TEST_ASSERT_FALSE(f.st.ringVisible);
  FocusResult r = f.key(LogicalKey::Next);  // acts and reveals the ring
  TEST_ASSERT_TRUE(r.effect == Effect::TabChanged);
  TEST_ASSERT_EQUAL_INT(1, f.st.tab);
  TEST_ASSERT_TRUE(f.st.ringVisible);
  // PUSH as first event: enters WIDGET mode, ring appears
  Fx g(P, 1, {W(WidgetType::Light)});
  r = g.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(g.st.mode == FocusMode::Widget);
  TEST_ASSERT_TRUE(g.st.ringVisible);
  // LONG_PUSH as first event: opens Settings, no ring
  Fx h(P, 1, {W(WidgetType::Light)});
  r = h.key(LogicalKey::LongPush);
  TEST_ASSERT_TRUE(r.effect == Effect::OpenSettings);
  TEST_ASSERT_FALSE(h.st.ringVisible);
  TEST_ASSERT_TRUE(h.st.mode == FocusMode::Tab);
  // with a visible ring LONG_PUSH leaves it visible
  h.st.ringVisible = true;
  r = h.key(LogicalKey::LongPush);
  TEST_ASSERT_TRUE(r.effect == Effect::OpenSettings);
  TEST_ASSERT_TRUE(h.st.ringVisible);
  // LONG_PUSH in WIDGET mode goes to TAB, in INNER to WIDGET
  Fx k(P, 1, {W(WidgetType::Cover)});
  k.key(LogicalKey::Push);
  k.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(k.st.mode == FocusMode::Inner);
  TEST_ASSERT_TRUE(k.key(LogicalKey::LongPush).effect == Effect::FocusMoved);
  TEST_ASSERT_TRUE(k.st.mode == FocusMode::Widget);
  TEST_ASSERT_TRUE(k.key(LogicalKey::LongPush).effect == Effect::FocusMoved);
  TEST_ASSERT_TRUE(k.st.mode == FocusMode::Tab);
}
void test_focus_inner_entry_points() {
  Fx f(P, 1, {W(WidgetType::Cover), W(WidgetType::Climate), W(WidgetType::Vacuum)});
  TEST_ASSERT_EQUAL_INT(3, (int)innerStops(f.env, 0).size());
  TEST_ASSERT_EQUAL_INT(2, (int)innerStops(f.env, 1).size());  // climate: minus, plus
  TEST_ASSERT_EQUAL_INT(3, (int)innerStops(f.env, 2).size());
  f.key(LogicalKey::Push);
  f.key(LogicalKey::Next);
  f.key(LogicalKey::Push);
  TEST_ASSERT_EQUAL_INT(1, f.st.inner);  // climate entry = minus
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

// ---- mockup match: docs/ui/tab-portrait.svg / tab-landscape.svg ----
// Expected rects are the translate(...) origins and the w-N-frame sizes (+2 for the 1 px inset)
// drawn in the SVGs. w-N is the N-th widget of workedExample() (config order).
void test_mockup_portrait() {
  std::vector<Tab> t = tabsOf(5);  // svg: 5 tabs configured > 4 slots, slot 3 = overflow "1/2"
  t[0].widgets = workedExample();
  Layout l = computeLayout(P, t, 0, 0);
  eqRect({0, 0, 540, 56}, l.statusBar);
  eqRect({0, 880, 540, 80}, l.tabBar);  // svg: rule (0,880,540,2) is its top edge
  TEST_ASSERT_FALSE(l.pager.visible);
  TEST_ASSERT_EQUAL_INT(10, (int)l.widgets.size());
  Rect e[] = {{16, 68, 508, 104},   // w-1 cover
              {16, 184, 508, 104},  // w-2 group
              {16, 300, 248, 104},  // w-3 light Ceiling
              {276, 300, 248, 104}, // w-4 light Floor lamp
              {16, 416, 248, 104},  // w-5 switch
              {16, 532, 508, 104},  // w-6 climate
              {276, 416, 248, 104}, // w-7 binary sensor
              {16, 648, 248, 104},  // w-8 sensor
              {276, 648, 248, 104}, // w-9 automation
              {16, 764, 508, 104}}; // w-10 weather
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT_EQUAL_INT(i, l.widgets[i].index);
    eqRect(e[i], l.widgets[i].rect);
  }
  // svg slots: active (16,892,118,56); others stroked at (147,893,116,54) = slot (146,892,118,56)
  Rect s[] = {{16, 892, 118, 56}, {146, 892, 118, 56}, {276, 892, 118, 56}, {406, 892, 118, 56}};
  for (int i = 0; i < 4; ++i) eqRect(s[i], l.tabSlots[i].rect);
  for (int i = 0; i < 3; ++i) TEST_ASSERT_TRUE(l.tabSlots[i].kind == SlotKind::Tab);
  TEST_ASSERT_TRUE(l.tabSlots[3].kind == SlotKind::Overflow);
  // status elements of the svg: battery cluster inside the settings hit area, indicator between
  TEST_ASSERT_TRUE(rectInside({428, 17, 40, 20}, l.settingsHit));
  TEST_ASSERT_TRUE(rectInside({472, 0, 52, 54}, l.settingsHit));
}
void test_mockup_landscape() {
  std::vector<Tab> t = tabsOf(5);  // svg: 5 tabs = 5 slots, no overflow
  t[0].widgets = workedExample();
  Layout l = computeLayout(L, t, 0, 0);
  eqRect({0, 0, 960, 64}, l.statusBar);
  eqRect({172, 0, 648, 64}, l.tabBar);
  TEST_ASSERT_FALSE(l.pager.visible);
  TEST_ASSERT_EQUAL_INT(10, (int)l.widgets.size());
  Rect e[] = {{16, 72, 552, 104},   // w-1 cover
              {16, 188, 552, 104},  // w-2 group
              {580, 72, 176, 104},  // w-3
              {768, 72, 176, 104},  // w-4
              {580, 188, 176, 104}, // w-5
              {16, 304, 552, 104},  // w-6 climate
              {768, 188, 176, 104}, // w-7 binary sensor
              {580, 304, 176, 104}, // w-8
              {768, 304, 176, 104}, // w-9
              {16, 420, 364, 104}}; // w-10 weather
  for (int i = 0; i < 10; ++i) eqRect(e[i], l.widgets[i].rect);
  int xs[] = {172, 304, 436, 568, 700};
  for (int i = 0; i < 5; ++i) {
    eqRect({xs[i], 4, 120, 56}, l.tabSlots[i].rect);
    TEST_ASSERT_TRUE(l.tabSlots[i].kind == SlotKind::Tab);
  }
  TEST_ASSERT_TRUE(rectInside({848, 22, 40, 20}, l.settingsHit));  // svg battery icon
  TEST_ASSERT_TRUE(rectInside({892, 4, 52, 56}, l.settingsHit));
}
// Sub-region frames of the svg buttons: w-1-up-btn (324,20,64,64) in a 552 wide landscape cover.
void test_mockup_buttons() {
  std::vector<Tab> t = tabsOf(1);
  t[0].widgets = workedExample();
  Layout l = computeLayout(L, t, 0, 0);
  auto s = subRegions(l.widgets[0].type, {3, 1}, l.widgets[0].rect);
  eqRect({16 + 324, 72 + 20, 64, 64}, s[0].rect);
}
static void checkLayoutInvariants(const Layout &l);
// ---- landscape 4 columns (docs/ui/proposal-landscape-4col.svg) ----
void test_grid_landscape4() {
  const Grid &g = gridFor(L, 4);
  TEST_ASSERT_EQUAL_INT(4, g.cols);
  TEST_ASSERT_EQUAL_INT(223, g.cellW);
  const int xs[4] = {16, 251, 486, 721};
  for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_INT(xs[i], g.colX[i]);
  TEST_ASSERT_EQUAL_INT(960, 16 + 4 * 223 + 3 * kGap + 16);
  TEST_ASSERT_EQUAL_INT(5, gridFor(L).cols);
  TEST_ASSERT_EQUAL_INT(5, gridFor(L, 7).cols);
  TEST_ASSERT_EQUAL_INT(2, gridFor(P, 4).cols);  // portrait ignores the option
  eqRect({16, 72, 458, 104}, cellRect(g, 0, 0, 2, 1));
  eqRect({486, 72, 458, 104}, cellRect(g, 2, 0, 2, 1));
  eqRect({721, 304, 223, 104}, cellRect(g, 3, 2, 1, 1));
}
void test_sizes_landscape4() {
  for (WidgetType t : {WidgetType::Cover, WidgetType::Climate, WidgetType::Vacuum}) {
    TEST_ASSERT_EQUAL_INT(2, sizeLimits(t, L, 4).def.w);
    TEST_ASSERT_EQUAL_INT(3, sizeLimits(t, L, 5).def.w);
    TEST_ASSERT_EQUAL_INT(3, sizeLimits(t, L).def.w);
    TEST_ASSERT_EQUAL_INT(2, sizeLimits(t, P, 4).def.w);
  }
}
// Proposal tab: 2 covers (row 0), 2 climates (row 1), 4 toggles (row 2), row 3 free.
void test_flow_landscape4_proposal() {
  std::vector<Widget> w = {W(WidgetType::Cover), W(WidgetType::Cover), W(WidgetType::Climate),
                           W(WidgetType::Climate)};
  for (int i = 0; i < 4; ++i) w.push_back(W(WidgetType::Light));
  Flow f = computeFlow(L, w, 4);
  TEST_ASSERT_EQUAL_INT(0, (int)f.clampReports.size());
  TEST_ASSERT_FALSE(f.pagerVisible);
  eqRect({16, 72, 458, 104}, f.placements[0].rect);
  eqRect({486, 72, 458, 104}, f.placements[1].rect);
  eqRect({16, 188, 458, 104}, f.placements[2].rect);
  eqRect({486, 188, 458, 104}, f.placements[3].rect);
  const int xs[4] = {16, 251, 486, 721};
  for (int i = 0; i < 4; ++i) eqRect({xs[i], 304, 223, 104}, f.placements[4 + i].rect);
  // sub-regions in the 458 px widgets: cover buttons W-228.., climate minus W-276, plus W-76
  auto cs = subRegions(WidgetType::Cover, {2, 1}, f.placements[0].rect);
  eqRect({16 + 230, 92, 64, 64}, cs[0].rect);
  eqRect({16 + 382, 92, 64, 64}, cs[2].rect);
  auto ks = subRegions(WidgetType::Climate, {2, 1}, f.placements[3].rect);
  eqRect({486 + 182, 208, 64, 64}, ks[0].rect);
  eqRect({486 + 382, 208, 64, 64}, ks[1].rect);
}
void test_layout_uses_tab_columns() {
  std::vector<Tab> t = tabsOf(2);
  t[0].landscapeColumns = 4;
  t[0].widgets = {W(WidgetType::Cover), W(WidgetType::Cover)};
  t[1].widgets = {W(WidgetType::Cover), W(WidgetType::Cover)};
  Layout a = computeLayout(L, t, 0, 0);
  eqRect({486, 72, 458, 104}, a.widgets[1].rect);
  Layout b = computeLayout(L, t, 1, 0);
  eqRect({16, 188, 552, 104}, b.widgets[1].rect);  // 3 + 3 > 5: second cover on row 1
  Layout p = computeLayout(P, t, 0, 0);
  eqRect({16, 68, 508, 104}, p.widgets[0].rect);
  Hit h = hitTest(a, {486 + 382 + 10, 92 + 10});
  TEST_ASSERT_TRUE(h.kind == HitKind::Widget && h.index == 1 && h.sub == 3);
}
void test_pager_landscape4() {
  std::vector<Widget> w(13, W(WidgetType::Light));  // 4 x 4 = 16 cells fit, 4 rows; 17th pages
  w.resize(17, W(WidgetType::Light));
  Flow f = computeFlow(L, w, 4);
  TEST_ASSERT_TRUE(f.pagerVisible);
  TEST_ASSERT_EQUAL_INT(2, f.pageCount);  // 12 cells per page with the pager row
  for (const auto &p : f.placements) TEST_ASSERT_TRUE(p.rect.y + p.rect.h <= 420);
}
void test_property_both_landscape_grids() {
  std::mt19937 rng(43);
  const WidgetType ts[] = {WidgetType::Cover, WidgetType::Light, WidgetType::Climate, WidgetType::Sensor,
                           WidgetType::Weather, WidgetType::Vacuum, WidgetType::Switch};
  for (int it = 0; it < 300; ++it) {
    for (int cols : {4, 5}) {
      std::vector<Tab> t = tabsOf(1);
      t[0].landscapeColumns = cols;
      int n = 1 + int(rng() % 24);
      for (int i = 0; i < n; ++i) {
        Widget w = W(ts[rng() % 7]);
        if (rng() % 3 == 0) w.size = config::WidgetSize{1 + int(rng() % 4), 1 + int(rng() % 4)};
        t[0].widgets.push_back(w);
      }
      Layout probe = computeLayout(L, t, 0, 0);
      for (int sp = 0; sp < probe.pager.subPageCount; ++sp) {
        Layout l = computeLayout(L, t, 0, sp);
        checkLayoutInvariants(l);
        for (const auto &pl : l.widgets) TEST_ASSERT_TRUE(pl.cellX + pl.cellW <= cols);
      }
    }
  }
}

// config.example.yaml parses with no errors and no clamp reports in both orientations.
void test_example_config_no_clamps() {
  std::ifstream f("config.example.yaml");
  std::stringstream ss;
  ss << f.rdbuf();
  TEST_ASSERT_FALSE_MESSAGE(ss.str().empty(), "run tests from the repo root");
  config::ParseResult r = config::parseConfig(ss.str());
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_TRUE(r.config.tabs.size() >= 2);
  for (Orientation o : {P, L})
    for (const auto &tab : r.config.tabs) {
      Flow fl = computeFlow(o, tab.widgets);
      TEST_ASSERT_EQUAL_INT(0, (int)fl.clampReports.size());
      TEST_ASSERT_EQUAL_INT((int)tab.widgets.size(), (int)fl.placements.size());
    }
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
      for (int x = -5; x < 970; x += 7) {
        HitKind k = hitTest(l, {x, y}).kind;  // only the two status areas are live without tabs
        if (l.refreshHit.contains(x, y)) TEST_ASSERT_TRUE(k == HitKind::Refresh);
        else if (l.settingsHit.contains(x, y)) TEST_ASSERT_TRUE(k == HitKind::Settings);
        else TEST_ASSERT_TRUE(k == HitKind::None);
      }
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
  // every wide widget requested 2x2: clamped to one row (reported), still never overlaps
  for (Orientation o : {P, L}) {
    std::vector<Tab> t = tabsOf(1);
    WidgetType types[] = {WidgetType::Cover, WidgetType::Climate, WidgetType::Vacuum};
    for (int i = 0; i < 24; ++i) t[0].widgets.push_back(W(types[i % 3], 2, 2));
    Layout base = computeLayout(o, t, 0, 0);
    // pager mode: portrait 6 wide per page (4 pages); landscape 3 rows, 1 wide per row (8 pages)
    int per = o == P ? 6 : 3;
    TEST_ASSERT_EQUAL_INT(24 / per, base.pager.subPageCount);
    TEST_ASSERT_TRUE(base.pager.visible);
    TEST_ASSERT_EQUAL_INT(24, (int)base.flow.clampReports.size());
    for (int p = 0; p < base.pager.subPageCount; ++p) {
      Layout l = computeLayout(o, t, 0, p);
      TEST_ASSERT_EQUAL_INT(per, (int)l.widgets.size());
      TEST_ASSERT_EQUAL_INT(per * p, l.widgets[0].index);
      checkLayoutInvariants(l);
    }
    // 2x2 light: clamped to 1x1 (type max) and reported, every one of them
    std::vector<Widget> lights(24, W(WidgetType::Light, 2, 2));
    Flow f = computeFlow(o, lights);
    TEST_ASSERT_EQUAL_INT(24, (int)f.clampReports.size());
    for (auto &c : f.clampReports) {
      TEST_ASSERT_EQUAL_INT(1, c.used.w);
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

// Regression for ui_rocker.cpp:107: a FocusState.tab below -tabCount used to wrap to a negative index.
void test_robust_focus_negative_tab_known_bug() {
  Flow fl;
  FocusEnv env;
  env.tabCount = 3;
  env.flow = &fl;
  for (int tab : {-10, -4, -3, INT_MIN, INT_MAX})
    for (LogicalKey k : {LogicalKey::Prev, LogicalKey::Next}) {
      FocusState st;
      st.tab = tab;
      focusHandle(st, env, k);
      TEST_ASSERT_TRUE(st.tab >= 0 && st.tab < 3);
    }
  FocusState st;
  st.tab = -10;
  focusHandle(st, env, LogicalKey::Prev);
  TEST_ASSERT_EQUAL_INT(1, st.tab);  // ((-10 - 1) mod 3) = 1
}


// ======================= tester additions (task 011 verification) =======================

// Brute-force hit oracle: every pixel of the screen, expected kind computed from the spec tables
// of layout.md section 2/6 (written out here, not read from the Layout).
static void bruteForceStatusAndTabs(Orientation o, int nTabs) {
  Layout l = computeLayout(o, tabsOf(nTabs), 0, 0);
  Rect refresh = o == P ? Rect{0, 0, 288, 56} : Rect{16, 4, 144, 56};
  Rect settingsR = o == P ? Rect{412, 0, 112, 56} : Rect{832, 4, 112, 56};
  std::vector<Rect> slots;
  if (o == P) for (int x : {16, 146, 276, 406}) slots.push_back({x, 892, 118, 56});
  else for (int x : {172, 304, 436, 568, 700}) slots.push_back({x, 4, 120, 56});
  int nSlots = (int)slots.size();
  int per = nTabs <= nSlots ? nSlots : nSlots - 1;
  int w = o == P ? 540 : 960, h = o == P ? 960 : 540;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      HitKind exp = HitKind::None;
      int idx = 0;
      for (int i = 0; i < nSlots; ++i)
        if (slots[i].contains(x, y)) {
          if (i < per && i < nTabs) { exp = HitKind::Tab; idx = i; }
          else if (nTabs > nSlots && i == nSlots - 1) exp = HitKind::TabOverflow;
        }
      if (refresh.contains(x, y)) exp = HitKind::Refresh;
      if (settingsR.contains(x, y)) exp = HitKind::Settings;
      Hit g = hitTest(l, {x, y});
      if (g.kind != exp || g.index != idx || g.sub != 0) {
        char msg[96];
        snprintf(msg, sizeof msg, "o=%d tabs=%d at (%d,%d): got %d/%d want %d/%d", (int)o, nTabs, x, y, (int)g.kind,
                 g.index, (int)exp, idx);
        TEST_FAIL_MESSAGE(msg);
      }
    }
}
void test_t011_hit_bruteforce_status_and_tabs() {
  for (Orientation o : {P, L})
    for (int n : {0, 1, 4, 5, 6, 16}) bruteForceStatusAndTabs(o, n);
}

// Hits on a widget that is on another sub-page must not report that widget (old test was vacuous).
void test_t011_hit_other_subpage_widget() {
  std::vector<Tab> t = tabsOf(1);
  for (int i = 0; i < 15; ++i) t[0].widgets.push_back(W(WidgetType::Light));  // 12 + 3
  Layout p0 = computeLayout(P, t, 0, 0);
  Layout p1 = computeLayout(P, t, 0, 1);
  TEST_ASSERT_EQUAL_INT(12, (int)p0.widgets.size());
  TEST_ASSERT_EQUAL_INT(3, (int)p1.widgets.size());
  expectHit(p0, 16, 68, HitKind::Widget, 0, 0);
  expectHit(p1, 16, 68, HitKind::Widget, 12, 0);
  expectHit(p1, 276, 68, HitKind::Widget, 13, 0);
  expectHit(p1, 276, 184, HitKind::None);  // slot (1,1) of page 1 is empty
  expectHit(p1, 16, 300, HitKind::None);
  // every point with a Widget hit belongs to a widget on this page
  for (int y = 0; y < 960; y += 3)
    for (int x = 0; x < 540; x += 3) {
      Hit h = H(p1, x, y);
      if (h.kind == HitKind::Widget) TEST_ASSERT_TRUE(h.index >= 12 && h.index <= 14);
    }
}

// Climate and cover sub-region hits in landscape (8.4: cover L 340/416/492 at y=92 relative to (16,72);
// climate L minus (292,324), plus (492,324) for the widget at (16,304,552,104)).
void test_t011_hit_subregions_landscape() {
  std::vector<Tab> t = tabsOf(1);
  t[0].widgets = {W(WidgetType::Cover), W(WidgetType::Light), W(WidgetType::Light), W(WidgetType::Climate)};
  // cover (0,0) 3 wide; lights at (3,0),(4,0); climate (0,1)
  Layout l = computeLayout(L, t, 0, 0);
  expectHit(l, 340, 92, HitKind::Widget, 0, 1);
  expectHit(l, 403, 155, HitKind::Widget, 0, 1);
  expectHit(l, 404, 120, HitKind::Widget, 0, 0);  // 12 px gap
  expectHit(l, 416, 92, HitKind::Widget, 0, 2);
  expectHit(l, 492, 92, HitKind::Widget, 0, 3);
  expectHit(l, 555, 155, HitKind::Widget, 0, 3);
  expectHit(l, 556, 120, HitKind::Widget, 0, 0);
  // climate at (16,188,552,104): minus at x = 16+276 = 292, y = 188+20 = 208
  expectHit(l, 292, 208, HitKind::Widget, 3, 1);
  expectHit(l, 355, 271, HitKind::Widget, 3, 1);
  expectHit(l, 356, 240, HitKind::Widget, 3, 0);
  expectHit(l, 492, 208, HitKind::Widget, 3, 2);
  expectHit(l, 555, 271, HitKind::Widget, 3, 2);
  expectHit(l, 424, 240, HitKind::Widget, 3, 0);  // value area
  // climate has exactly two sub-regions: nothing else answers a sub id
  for (int y = 188; y < 292; ++y)
    for (int x = 16; x < 568; ++x) TEST_ASSERT_TRUE(H(l, x, y).sub >= 0 && H(l, x, y).sub <= 2);
}

// Spacing between sub-regions is exactly the spec value (12 px between buttons), not just "some gap".
void test_t011_subregion_spacing_exact() {
  Rect w[] = {{16, 68, 508, 104}, {16, 72, 552, 104}, {276, 188, 508, 104}};
  for (Rect r : w) {
    for (WidgetType t : {WidgetType::Cover, WidgetType::Vacuum}) {
      auto s = subRegions(t, {2, 1}, r);
      TEST_ASSERT_EQUAL_INT(3, (int)s.size());
      TEST_ASSERT_EQUAL_INT(r.x + r.w - 228, s[0].rect.x);
      TEST_ASSERT_EQUAL_INT(r.x + r.w - 152, s[1].rect.x);
      TEST_ASSERT_EQUAL_INT(r.x + r.w - 76, s[2].rect.x);
      TEST_ASSERT_EQUAL_INT(12, s[1].rect.x - (s[0].rect.x + s[0].rect.w));
      TEST_ASSERT_EQUAL_INT(12, s[2].rect.x - (s[1].rect.x + s[1].rect.w));
      for (auto &x : s) TEST_ASSERT_EQUAL_INT(r.y + 20, x.rect.y);
      TEST_ASSERT_EQUAL_INT(12, r.x + r.w - (s[2].rect.x + s[2].rect.w));  // right padding
    }
    auto c = subRegions(WidgetType::Climate, {2, 1}, r);
    TEST_ASSERT_EQUAL_INT(2, (int)c.size());
    TEST_ASSERT_EQUAL_INT(r.x + r.w - 276, c[0].rect.x);
    TEST_ASSERT_EQUAL_INT(r.x + r.w - 76, c[1].rect.x);
  }
  // degenerate input must not crash
  (void)subRegions(WidgetType::Cover, {0, 0}, {0, 0, 0, 0});
  (void)subRegions(WidgetType::Climate, {-5, 99}, {-100, -100, 10, 10});
}

// sleepIndicatorRect agrees with Layout in every orientation and under any tab count / state.
void test_t011_sleep_indicator_stable() {
  for (Orientation o : {P, L})
    for (int n : {0, 1, 5, 16}) {
      Layout l = computeLayout(o, tabsOf(n), n / 2, 3, 2);
      eqRect(sleepIndicatorRect(o), l.sleepIndicator);
      eqRect(gridFor(o).sleepIndicator, l.sleepIndicator);
    }
}

// Rocker mode machine, identical in both orientations (layout.md 7.2): TAB -> WIDGET -> INNER and back,
// LONG_PUSH in TAB opens Settings without touching tab / group / sub-page / mode.
void test_t011_rocker_modes_both_orientations() {
  for (Orientation o : {P, L}) {
    Fx f(o, 3, {W(WidgetType::Light), W(WidgetType::Climate), W(WidgetType::Cover)});
    // wake: no ring; first event acts and shows the ring
    TEST_ASSERT_FALSE(f.st.ringVisible);
    FocusResult r = f.key(LogicalKey::Push);  // TAB -> WIDGET on the first focusable widget
    TEST_ASSERT_TRUE(r.effect == Effect::FocusMoved);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
    TEST_ASSERT_TRUE(f.st.ringVisible);
    TEST_ASSERT_EQUAL_INT(0, focusedWidget(f.st, f.env));
    r = f.key(LogicalKey::Push);  // single action
    TEST_ASSERT_TRUE(r.effect == Effect::WidgetAction);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
    f.key(LogicalKey::Next);  // climate
    r = f.key(LogicalKey::Push);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Inner);
    TEST_ASSERT_EQUAL_INT(1, f.st.inner);
    r = f.key(LogicalKey::Push);
    TEST_ASSERT_TRUE(r.effect == Effect::WidgetAction);
    TEST_ASSERT_EQUAL_INT(1, r.widgetIndex);
    TEST_ASSERT_EQUAL_INT(1, r.sub);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Inner);
    f.key(LogicalKey::Next);  // plus
    r = f.key(LogicalKey::Push);
    TEST_ASSERT_EQUAL_INT(2, r.sub);
    f.key(LogicalKey::LongPush);  // INNER -> WIDGET (same widget)
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
    TEST_ASSERT_EQUAL_INT(1, focusedWidget(f.st, f.env));
    f.key(LogicalKey::LongPush);  // WIDGET -> TAB
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
    TEST_ASSERT_EQUAL_INT(0, f.st.stop);
    // LONG_PUSH in TAB: OpenSettings, nothing else changes
    FocusState before = f.st;
    r = f.key(LogicalKey::LongPush);
    TEST_ASSERT_TRUE(r.effect == Effect::OpenSettings);
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
    TEST_ASSERT_EQUAL_INT(before.tab, f.st.tab);
    TEST_ASSERT_EQUAL_INT(before.group, f.st.group);
    TEST_ASSERT_EQUAL_INT(before.subPage, f.st.subPage);
    TEST_ASSERT_EQUAL_INT(before.stop, f.st.stop);
    // end of list keeps the sub-page and lands in TAB mode with the ring on the tab
    f.key(LogicalKey::Push);
    f.key(LogicalKey::Next);
    f.key(LogicalKey::Next);
    r = f.key(LogicalKey::Next);  // after the last widget
    TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
    TEST_ASSERT_TRUE(f.st.ringVisible);
    TEST_ASSERT_EQUAL_INT(0, f.st.subPage);
    TEST_ASSERT_EQUAL_INT(0, f.st.tab);
  }
}

// First event after wake in both orientations, all four keys.
void test_t011_first_event_after_wake_both() {
  for (Orientation o : {P, L}) {
    {
      Fx f(o, 3, {W(WidgetType::Light)});
      FocusResult r = f.key(LogicalKey::Prev);  // wraps 0 -> 2
      TEST_ASSERT_TRUE(r.effect == Effect::TabChanged);
      TEST_ASSERT_EQUAL_INT(2, f.st.tab);
      TEST_ASSERT_TRUE(f.st.ringVisible);
    }
    {
      Fx f(o, 3, {W(WidgetType::Light)});
      f.key(LogicalKey::Next);
      TEST_ASSERT_EQUAL_INT(1, f.st.tab);
      TEST_ASSERT_TRUE(f.st.ringVisible);
    }
    {
      Fx f(o, 1, {W(WidgetType::Light)});
      f.key(LogicalKey::Push);
      TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
      TEST_ASSERT_TRUE(f.st.ringVisible);
    }
    {
      Fx f(o, 1, {W(WidgetType::Light)});
      FocusResult r = f.key(LogicalKey::LongPush);
      TEST_ASSERT_TRUE(r.effect == Effect::OpenSettings);
      TEST_ASSERT_FALSE(f.st.ringVisible);
    }
  }
}

// End-of-list sub-page rules (8.4) in landscape as well (the old tests only ran them in portrait).
void test_t011_subpage_end_rules_landscape() {
  // landscape pager mode: 15 cells per page. 20 sensors + a light (21 cells > 20, so the pager is on):
  // page 0 has no focusable widget, the light is on page 1.
  std::vector<Widget> w(20, W(WidgetType::Sensor));
  w.push_back(W(WidgetType::Light));  // page 1
  Fx f(L, 1, w);
  TEST_ASSERT_EQUAL_INT(2, f.flow.pageCount);
  // PREV on the first widget of page 1: no earlier focusable page -> TAB, sub-page stays 1
  f.st.subPage = 1;
  f.st.mode = FocusMode::Widget;
  f.st.stop = 1;
  f.st.ringVisible = true;
  f.key(LogicalKey::Prev);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  TEST_ASSERT_EQUAL_INT(1, f.st.subPage);
  // PUSH re-enters on the kept sub-page; NEXT is the end, sub-page stays 1
  f.key(LogicalKey::Push);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Widget);
  TEST_ASSERT_EQUAL_INT(20, focusedWidget(f.st, f.env));
  f.key(LogicalKey::Next);
  TEST_ASSERT_TRUE(f.st.mode == FocusMode::Tab);
  TEST_ASSERT_EQUAL_INT(1, f.st.subPage);
}

// Touch interplay (7.2): helpers in both orientations and with out-of-range arguments.
void test_t011_touch_interplay_extra() {
  for (Orientation o : {P, L}) {
    int per = gridFor(o).tabSlots - 1;
    for (FocusMode m : {FocusMode::Tab, FocusMode::Widget, FocusMode::Inner}) {
      FocusState st;
      st.mode = m;
      st.subPage = 3;
      st.stop = 2;
      st.inner = 2;
      focusTouchTab(st, o, 16, 13);
      TEST_ASSERT_TRUE(st.mode == FocusMode::Tab);
      TEST_ASSERT_EQUAL_INT(13, st.tab);
      TEST_ASSERT_EQUAL_INT(13 / per, st.group);
      TEST_ASSERT_EQUAL_INT(0, st.subPage);
      TEST_ASSERT_EQUAL_INT(0, st.stop);
    }
    // overflow wraps through all groups and never changes tab / mode
    FocusState st;
    st.tab = 2;
    st.mode = FocusMode::Widget;
    int G = groupCount(o, 16);
    for (int i = 0; i < G; ++i) focusOverflow(st, o, 16);
    TEST_ASSERT_EQUAL_INT(0, st.group);
    TEST_ASSERT_EQUAL_INT(2, st.tab);
    TEST_ASSERT_TRUE(st.mode == FocusMode::Widget);
    // overflow with no tabs / one group stays at 0
    FocusState e;
    focusOverflow(e, o, 0);
    TEST_ASSERT_EQUAL_INT(0, e.group);
    focusOverflow(e, o, 1);
    TEST_ASSERT_EQUAL_INT(0, e.group);
    // pager step: no wrap, no change when out of range, returns to TAB mode when it moves
    FocusState p;
    p.mode = FocusMode::Inner;
    p.subPage = 0;
    TEST_ASSERT_FALSE(focusPagerStep(p, -1, 3));
    TEST_ASSERT_TRUE(p.mode == FocusMode::Inner);  // untouched when nothing changed
    TEST_ASSERT_EQUAL_INT(0, p.subPage);
    TEST_ASSERT_TRUE(focusPagerStep(p, 1, 3));
    TEST_ASSERT_TRUE(p.mode == FocusMode::Tab);
    TEST_ASSERT_EQUAL_INT(1, p.subPage);
    TEST_ASSERT_FALSE(focusPagerStep(p, 2, 3));
    TEST_ASSERT_FALSE(focusPagerStep(p, 1, 1));
    TEST_ASSERT_FALSE(focusPagerStep(p, 1, 0));
    TEST_ASSERT_EQUAL_INT(1, p.subPage);
  }
  // after a touch tab tap a rocker NEXT continues from that tab
  Fx f(P, 8, {});
  focusTouchTab(f.st, P, 8, 5);
  f.key(LogicalKey::Next);
  TEST_ASSERT_EQUAL_INT(6, f.st.tab);
}

// The refresh and settings hit rects keep working with a visible pager and with a focused ring.
void test_t011_status_hits_with_pager() {
  std::vector<Tab> t = tabsOf(5);
  for (int i = 0; i < 30; ++i) t[0].widgets.push_back(W(WidgetType::Light));
  for (Orientation o : {P, L}) {
    Layout l = computeLayout(o, t, 0, 1);
    TEST_ASSERT_TRUE(l.pager.visible);
    expectHit(l, l.refreshHit.x, l.refreshHit.y, HitKind::Refresh);
    expectHit(l, l.refreshHit.x + l.refreshHit.w - 1, l.refreshHit.y + l.refreshHit.h - 1, HitKind::Refresh);
    expectHit(l, l.settingsHit.x, l.settingsHit.y, HitKind::Settings);
    expectHit(l, l.settingsHit.x + l.settingsHit.w - 1, l.settingsHit.y + l.settingsHit.h - 1, HitKind::Settings);
    expectHit(l, l.refreshHit.x + l.refreshHit.w, l.refreshHit.y, HitKind::None);
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_areas_portrait);
  RUN_TEST(test_areas_landscape);
  RUN_TEST(test_closing_sums);
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
  RUN_TEST(test_size_clamp_rows_synthetic);
  RUN_TEST(test_flow_worked_example);
  RUN_TEST(test_flow_empty_tab);
  RUN_TEST(test_flow_capacity);
  RUN_TEST(test_flow_row_fill);
  RUN_TEST(test_flow_overflow_portrait_wide);
  RUN_TEST(test_flow_gap_fill_current_page_only);
  RUN_TEST(test_flow_no_backfill_earlier_page);
  RUN_TEST(test_subregions_cover);
  RUN_TEST(test_subregions_climate);
  RUN_TEST(test_subregions_none_and_geometry);
  RUN_TEST(test_property_no_overlap);
  RUN_TEST(test_hit_status_portrait);
  RUN_TEST(test_hit_status_landscape);
  RUN_TEST(test_hit_tabs_and_edges);
  RUN_TEST(test_hit_widget_gaps);
  RUN_TEST(test_hit_subregions);
  RUN_TEST(test_hit_pager);
  RUN_TEST(test_hit_dirty_inflation);
  RUN_TEST(test_rocker_mapping);
  RUN_TEST(test_focus_tab_mode_wrap_and_groups);
  RUN_TEST(test_focus_order_and_skip_readonly);
  RUN_TEST(test_focus_history_sensors);
  RUN_TEST(test_focus_row_col_order);
  RUN_TEST(test_focus_unavailable_skipped);
  RUN_TEST(test_focus_single_action_and_inner);
  RUN_TEST(test_focus_subpages);
  RUN_TEST(test_focus_end_when_no_later_focusable_page);
  RUN_TEST(test_focus_end_when_no_earlier_focusable_page);
  RUN_TEST(test_focus_skips_pages_without_focusable);
  RUN_TEST(test_focus_first_event_after_wake);
  RUN_TEST(test_focus_inner_entry_points);
  RUN_TEST(test_focus_touch);
  RUN_TEST(test_focus_invert_flow);
  RUN_TEST(test_mockup_portrait);
  RUN_TEST(test_mockup_landscape);
  RUN_TEST(test_grid_landscape4);
  RUN_TEST(test_sizes_landscape4);
  RUN_TEST(test_flow_landscape4_proposal);
  RUN_TEST(test_layout_uses_tab_columns);
  RUN_TEST(test_pager_landscape4);
  RUN_TEST(test_property_both_landscape_grids);
  RUN_TEST(test_mockup_buttons);
  RUN_TEST(test_example_config_no_clamps);
  RUN_TEST(test_robust_zero_tabs);
  RUN_TEST(test_robust_16_tabs_24_widgets);
  RUN_TEST(test_robust_all_2x2);
  RUN_TEST(test_robust_nothing_focusable);
  RUN_TEST(test_robust_out_of_range_args);
  RUN_TEST(test_robust_hit_outside_screen);
  RUN_TEST(test_robust_focus_empty_and_null_flow);
  RUN_TEST(test_robust_focus_garbage_state);
  RUN_TEST(test_robust_focus_negative_tab_known_bug);
  RUN_TEST(test_t011_hit_bruteforce_status_and_tabs);
  RUN_TEST(test_t011_hit_other_subpage_widget);
  RUN_TEST(test_t011_hit_subregions_landscape);
  RUN_TEST(test_t011_subregion_spacing_exact);
  RUN_TEST(test_t011_sleep_indicator_stable);
  RUN_TEST(test_t011_rocker_modes_both_orientations);
  RUN_TEST(test_t011_first_event_after_wake_both);
  RUN_TEST(test_t011_subpage_end_rules_landscape);
  RUN_TEST(test_t011_touch_interplay_extra);
  RUN_TEST(test_t011_status_hits_with_pager);
  return UNITY_END();
}
