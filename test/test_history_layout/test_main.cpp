#include <unity.h>

#include <cmath>

#include "history_layout.h"

using namespace history;

void setUp() {}
void tearDown() {}

#define EQD(e, a) TEST_ASSERT_TRUE(std::fabs((e) - (a)) < 1e-9)
static int utc(int64_t) { return 0; }
static int plus1(int64_t) { return 3600; }
static const int64_t kNow = 1761696000 + 14 * 3600 + 5 * 60;  // 2025-10-29 14:05 UTC (Wednesday)

void test_rects() {
  GraphLayout p = graphLayout(ChartType::Line, false);
  TEST_ASSERT_EQUAL(392, p.columns());
  TEST_ASSERT_EQUAL(636, p.plot.bottom());
  TEST_ASSERT_EQUAL(91, p.axisLeft.x);
  TEST_ASSERT_EQUAL(269, p.axisLeft.h);
  TEST_ASSERT_EQUAL(394, p.axisBottom.w);
  TEST_ASSERT_EQUAL(84, p.yLabelRight);
  TEST_ASSERT_EQUAL(504, p.yTickCy[1]);
  TEST_ASSERT_EQUAL(658, p.xLabelCy);
  TEST_ASSERT_EQUAL(4, p.maxXTicks);
  GraphLayout l = graphLayout(ChartType::Bar, true);
  TEST_ASSERT_EQUAL(764, l.columns());
  TEST_ASSERT_EQUAL(108, l.yLabelRight);
  TEST_ASSERT_EQUAL(306, l.yTickCy[1]);
  TEST_ASSERT_EQUAL(115, l.axisBottom.x);
  TEST_ASSERT_EQUAL(766, l.axisBottom.w);
  TEST_ASSERT_EQUAL(426, l.axisBottom.y);
  TEST_ASSERT_EQUAL(8, l.maxXTicks);
}

void test_step_layout() {
  GraphLayout p = graphLayout(ChartType::Step, false);
  TEST_ASSERT_EQUAL(452, p.columns());
  TEST_ASSERT_EQUAL(490, p.xLabelCy);
  TEST_ASSERT_EQUAL(508, p.hairlineY);
  TEST_ASSERT_EQUAL(528, p.listCy0);
  TEST_ASSERT_EQUAL(5, p.maxXTicks);
  GraphLayout l = graphLayout(ChartType::Step, true);
  TEST_ASSERT_EQUAL(824, l.columns());
  TEST_ASSERT_EQUAL(304, l.xLabelCy);
  TEST_ASSERT_EQUAL(322, l.hairlineY);
  TEST_ASSERT_EQUAL(342, l.listCy0);
  TEST_ASSERT_EQUAL(9, l.maxXTicks);
}

void test_column_mapping() {
  TEST_ASSERT_EQUAL(0, timeToX(100, 100, 200, 392));
  TEST_ASSERT_EQUAL(196, timeToX(150, 100, 200, 392));
  TEST_ASSERT_EQUAL(392, timeToX(300, 100, 200, 392));
  GraphLayout p = graphLayout(ChartType::Line, false);
  Axis a = axis3(19.9, 22.7, false, 0, false, 0, true);
  TEST_ASSERT_EQUAL(636, valueY(p, a, a.lo));
  TEST_ASSERT_EQUAL(372, valueY(p, a, a.hi));
  TEST_ASSERT_EQUAL(504, valueY(p, a, a.ticks[1]));
  TEST_ASSERT_EQUAL(372, valueY(p, a, 1e9));
}

void test_axis3() {
  Axis a = axis3(19.9, 22.7, false, 0, false, 0, true);  // tighter than the mockup (19 / 21.5 / 24)
  TEST_ASSERT_EQUAL(3, (int)a.ticks.size());
  EQD(19.0, a.lo);
  EQD(23.0, a.hi);
  EQD(21.0, a.ticks[1]);
  TEST_ASSERT_EQUAL(0, a.decimals);
  Axis f = axis3(5, 7, true, 0, true, 100, true);  // both fixed
  EQD(50, f.ticks[1]);
  Axis flat = axis3(21, 21, false, 0, false, 0, true);
  TEST_ASSERT_TRUE(flat.lo < 21 && flat.hi > 21);
  Axis lo = axis3(3, 8, true, 0, false, 0, true);  // fixed min 0
  EQD(0, lo.lo);
  TEST_ASSERT_TRUE(lo.hi >= 8);
  Axis hi = axis3(3, 8, false, 0, true, 10, true);
  EQD(10, hi.hi);
  TEST_ASSERT_TRUE(hi.lo <= 3 - 0.2);
}

void test_bar_axis() {
  Axis a = barAxis(47.3, false, 0, false, 0, 264, true);
  EQD(0, a.lo);
  // the tallest bar plus 25 px label room fits
  TEST_ASSERT_TRUE((47.3 - a.lo) / (a.hi - a.lo) * 264 <= 264 - 25 + 1e-6);
  TEST_ASSERT_EQUAL(3, (int)a.ticks.size());
  Axis b = barAxis(47.3, false, 0, false, 0, 264, false);
  TEST_ASSERT_TRUE(b.hi >= 47.3);
}

void test_format() {
  TEST_ASSERT_EQUAL_STRING("21.5", formatValue(21.5, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("0.0", formatValue(-0.01, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("-3", formatValue(-3.2, 0).c_str());
}

void test_time_ticks_portrait() {
  auto t = timeTicks(kNow - 86400, kNow, 392, utc);  // 4 ticks max -> 6 h
  TEST_ASSERT_EQUAL(4, (int)t.size());
  TEST_ASSERT_EQUAL_STRING("18:00", t[0].label.c_str());
  TEST_ASSERT_EQUAL_STRING("00:00", t[1].label.c_str());
  TEST_ASSERT_EQUAL_STRING("06:00", t[2].label.c_str());
  TEST_ASSERT_EQUAL_STRING("12:00", t[3].label.c_str());
  TEST_ASSERT_TRUE(t[0].x > 0 && t[3].x < 392);
  TEST_ASSERT_EQUAL(21600, (int)timeTickStep(kNow - 86400, kNow, 392, utc));
}

void test_time_ticks_landscape_tz() {
  auto t = timeTicks(kNow - 86400, kNow, 764, utc);  // every 3 h
  TEST_ASSERT_EQUAL(8, (int)t.size());
  TEST_ASSERT_EQUAL_STRING("15:00", t[0].label.c_str());
  TEST_ASSERT_EQUAL_STRING("12:00", t[7].label.c_str());
  // UTC+1: ticks sit on local round hours
  auto l = timeTicks(kNow - 86400, kNow, 764, plus1);
  TEST_ASSERT_EQUAL(10800, (int)timeTickStep(kNow - 86400, kNow, 764, plus1));
  for (auto& k : l) {
    TEST_ASSERT_EQUAL(0, (int)((k.epoch + 3600) % 10800));
    TEST_ASSERT_TRUE(k.x >= 1 && k.x <= 763);
  }
  TEST_ASSERT_EQUAL_STRING("18:00", l[0].label.c_str());
}

void test_time_tick_step_ignores_position() {
  // 24 h portrait (4 ticks max): always 6 h, wherever the window sits (round hour, half past, TZ shift)
  for (int64_t off = 0; off < 86400; off += 1800) {
    TEST_ASSERT_EQUAL(21600, (int)timeTickStep(kNow - 86400 - off, kNow - off, 392, utc));
    TEST_ASSERT_EQUAL(21600, (int)timeTickStep(kNow - 86400 - off, kNow - off, 392, plus1));
    TEST_ASSERT_EQUAL(10800, (int)timeTickStep(kNow - 86400 - off, kNow - off, 764, utc));
  }
}

void test_time_ticks_days() {
  auto w = timeTicks(kNow - 7 * 86400, kNow, 764, utc);  // 7 d: daily ticks, weekday labels
  TEST_ASSERT_EQUAL(7, (int)w.size());
  TEST_ASSERT_EQUAL_STRING("Thu", w[0].label.c_str());
  TEST_ASSERT_EQUAL_STRING("Wed", w[6].label.c_str());
  auto m = timeTicks(kNow - 30 * 86400, kNow, 764, utc);  // beyond 8 d: d.m.
  TEST_ASSERT_TRUE(m.size() <= 8);
  TEST_ASSERT_TRUE(m[0].label.find('.') != std::string::npos);
  auto none = timeTicks(100, 100, 392, utc);
  TEST_ASSERT_EQUAL(0, (int)none.size());
}

void test_fit_labels() {
  auto k = fitLabels({10, 60, 70, 380, 120}, {40, 40, 40, 40, 40}, 0, 392, 8);
  // 10-20..30 left clipped (l=-10), 60 ok, 70 overlaps, 380 right clipped
  TEST_ASSERT_FALSE(k[0]);
  TEST_ASSERT_TRUE(k[1]);
  TEST_ASSERT_FALSE(k[2]);
  TEST_ASSERT_FALSE(k[3]);
  TEST_ASSERT_TRUE(k[4]);
}

static Series binSeries(std::initializer_list<int> v) {
  Series s;
  s.kind = Kind::Binary;
  s.start = 0;
  s.end = 100 * (int64_t)v.size();
  for (int x : v) {
    Column c;
    c.has = x >= 0;
    c.min = c.max = c.last = x > 0 ? 1.f : 0.f;
    s.columns.push_back(c);
  }
  s.points = 1;
  return s;
}

void test_lane_spans() {
  Series s = binSeries({0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1});
  auto sp = laneSpans(s);
  TEST_ASSERT_EQUAL(3, (int)sp.size());
  TEST_ASSERT_EQUAL(2, sp[0].x0);
  TEST_ASSERT_EQUAL(6, sp[0].x1);
  TEST_ASSERT_EQUAL(3, sp[1].x1 - sp[1].x0);  // 1 px event drawn 3 wide
  TEST_ASSERT_TRUE(sp[1].x0 <= 9 && sp[1].x1 >= 10);
  TEST_ASSERT_EQUAL(20, sp[2].x1);  // clamped at the right edge
  TEST_ASSERT_EQUAL(3, sp[2].x1 - sp[2].x0);
}

void test_changes() {
  Series s = binSeries({0, 0, 1, 1, 0, 0, 1, 1});
  auto c = lastChanges(s);
  TEST_ASSERT_EQUAL(3, (int)c.size());
  TEST_ASSERT_TRUE(c[0].on);
  TEST_ASSERT_EQUAL(600, c[0].epoch);
  TEST_ASSERT_FALSE(c[1].on);
  TEST_ASSERT_EQUAL(400, c[1].epoch);
  TEST_ASSERT_EQUAL(1, (int)lastChanges(s, 1).size());
  // blip inside one column: off -> on -> off within column 2
  Series b = binSeries({0, 0, 0, 0});
  b.columns[2].max = 1;
  auto bc = lastChanges(b);
  TEST_ASSERT_EQUAL(2, (int)bc.size());
  TEST_ASSERT_FALSE(bc[0].on);
  TEST_ASSERT_TRUE(bc[1].on);
  TEST_ASSERT_EQUAL(0, (int)lastChanges(binSeries({0, 0, 0})).size());
  StepSummary ss = stepSummary(s);
  TEST_ASSERT_EQUAL(2, ss.episodes);
  TEST_ASSERT_EQUAL(400, (int)ss.onSeconds);
}

void test_bars() {
  GraphLayout p = graphLayout(ChartType::Bar, false);
  auto b = barSlots(p.plot, 7);  // mockup: 7 slots of 56, bars 36 wide, 10 px either side
  TEST_ASSERT_EQUAL(7, (int)b.size());
  TEST_ASSERT_EQUAL(56, b[0].slotW);
  TEST_ASSERT_EQUAL(36, b[0].barW);
  TEST_ASSERT_EQUAL(92 + 10, b[0].barX);
  TEST_ASSERT_EQUAL(92 + 56 + 10, b[1].barX);
  TEST_ASSERT_TRUE(b[6].barX + b[6].barW <= p.plot.right());
  auto h = barSlots(p.plot, 24);
  TEST_ASSERT_TRUE(h[0].barW >= 1 && h[0].barW < h[0].slotW);
  TEST_ASSERT_TRUE(barLabels(8));
  TEST_ASSERT_FALSE(barLabels(9));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_rects);
  RUN_TEST(test_step_layout);
  RUN_TEST(test_column_mapping);
  RUN_TEST(test_axis3);
  RUN_TEST(test_bar_axis);
  RUN_TEST(test_format);
  RUN_TEST(test_time_ticks_portrait);
  RUN_TEST(test_time_ticks_landscape_tz);
  RUN_TEST(test_time_ticks_days);
  RUN_TEST(test_time_tick_step_ignores_position);
  RUN_TEST(test_fit_labels);
  RUN_TEST(test_lane_spans);
  RUN_TEST(test_changes);
  RUN_TEST(test_bars);
  return UNITY_END();
}
