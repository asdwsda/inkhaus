#include <unity.h>

#include <cstdio>
#include <string>

#include "history_layout.h"
#include "stats_parser.h"
#include "stats_plan.h"

using namespace history;

void setUp() {}
void tearDown() {}

namespace {
const int64_t kT0 = 1761696000;  // 2025-10-29T00:00:00Z

std::string iso(int64_t t) {
  int64_t days = t / 86400, rem = t % 86400;
  int64_t z = days + 719468, era = z / 146097, doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int64_t mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) ++y;
  char b[64];
  snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02d+00:00", (int)y, (int)m, (int)d, (int)(rem / 3600),
           (int)(rem / 60 % 60), (int)(rem % 60));
  return b;
}

// n buckets of `step` seconds from kT0; mean = 10 + i, min = mean - 1, max = mean + 1.
std::string body(const std::string& entity, int n, int64_t step, bool isoTimes, int skipMeanAt = -1) {
  std::string s = "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"" + entity + "\":[";
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    int64_t a = kT0 + i * step, b = a + step;
    char buf[256];
    if (isoTimes)
      snprintf(buf, sizeof buf, "{\"start\":\"%s\",\"end\":\"%s\",", iso(a).c_str(), iso(b).c_str());
    else
      snprintf(buf, sizeof buf, "{\"start\":%lld.0,\"end\":%lld,", (long long)a * 1000, (long long)b * 1000);
    s += buf;
    if (i == skipMeanAt) snprintf(buf, sizeof buf, "\"mean\":null,\"min\":null,\"max\":null,\"change\":null}");
    else snprintf(buf, sizeof buf, "\"mean\":%d.5,\"min\":%d.5,\"max\":%d.5,\"change\":0.25}", 10 + i, 9 + i, 11 + i);
    s += buf;
  }
  return s + "]}}";
}

StatsParams lineParams(int64_t hours, int cols) {
  StatsParams p;
  p.layout.start = kT0;
  p.layout.end = kT0 + hours * 3600;
  p.layout.columns = cols;
  return p;
}

StatsOutcome run(StatsParser& sp, const std::string& b, size_t chunk) {
  for (size_t i = 0; i < b.size(); i += chunk) sp.feed(b.data() + i, std::min(chunk, b.size() - i));
  return sp.finish();
}
}  // namespace

void test_huge_numbers_are_ignored() {
  StatsParams p = lineParams(1, 4);
  StatsParser sp("sensor.t", p);
  const std::string b = "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"sensor.t\":["
                        "{\"start\":1e300,\"end\":2e300,\"mean\":1.0},"
                        "{\"start\":1761696000000,\"end\":1761699600000,\"mean\":1e300,\"min\":-1e999},"
                        "{\"start\":1761696000000,\"end\":1761699600000,\"mean\":2.5}]}}";
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, b, 7));
  Series s = sp.series();
  TEST_ASSERT_EQUAL(1, (int)s.points);
  TEST_ASSERT_EQUAL_FLOAT(2.5f, s.columns[0].last);
}

void test_line_24h_5min_ms_and_iso() {
  for (int isoT = 0; isoT < 2; isoT++) {
    StatsParams p = lineParams(24, 288);
    p.periodSeconds = 300;
    StatsParser sp("sensor.t", p);
    TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, body("sensor.t", 288, 300, isoT, -1), 1000));
    Series s = sp.series();
    TEST_ASSERT_EQUAL(288, (int)s.columns.size());
    TEST_ASSERT_EQUAL(288, (int)s.points);
    for (int i = 0; i < 288; i++) {
      TEST_ASSERT_TRUE(s.columns[i].has);
      TEST_ASSERT_EQUAL_FLOAT(10.5f + i, s.columns[i].last);
      TEST_ASSERT_EQUAL_FLOAT(9.5f + i, s.columns[i].min);
      TEST_ASSERT_EQUAL_FLOAT(11.5f + i, s.columns[i].max);
    }
    TEST_ASSERT_EQUAL((int)Kind::Numeric, (int)s.kind);
  }
}

void test_line_7d_hour_spans_columns() {
  // 168 hourly buckets over 336 columns: each bucket covers 2 columns
  StatsParams p = lineParams(168, 336);
  StatsParser sp("sensor.t", p);
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, body("sensor.t", 168, 3600, false), 7));
  Series s = sp.series();
  for (int i = 0; i < 336; i++) {
    TEST_ASSERT_TRUE(s.columns[i].has);
    TEST_ASSERT_EQUAL_FLOAT(10.5f + i / 2, s.columns[i].last);
  }
}

void test_line_30d_day_fewer_columns_than_buckets() {
  // 30 daily buckets over 15 columns: later bucket of a column wins
  StatsParams p = lineParams(30 * 24, 15);
  StatsParser sp("sensor.t", p);
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, body("sensor.t", 30, 86400, true), 33));
  Series s = sp.series();
  for (int i = 0; i < 15; i++) {
    TEST_ASSERT_TRUE(s.columns[i].has);
    TEST_ASSERT_EQUAL_FLOAT(10.5f + 2 * i + 1, s.columns[i].last);
  }
}

void test_line_missing_values_and_gap() {
  StatsParams p = lineParams(24, 24);
  StatsParser sp("sensor.t", p);
  // hourly buckets 0..9, bucket 3 has null values; buckets 10.. absent
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, body("sensor.t", 10, 3600, false, 3), 64));
  Series s = sp.series();
  TEST_ASSERT_FALSE(s.columns[3].has);
  TEST_ASSERT_TRUE(s.columns[2].has);
  TEST_ASSERT_TRUE(s.columns[9].has);
  TEST_ASSERT_FALSE(s.columns[10].has);
  TEST_ASSERT_FALSE(s.columns[23].has);
  TEST_ASSERT_EQUAL(9, (int)s.points);
}

void test_line_mean_only_falls_back() {
  StatsParams p = lineParams(1, 4);
  std::string b = "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"sensor.t\":[{\"start\":" +
                  std::to_string(kT0 * 1000) + ",\"mean\":7}]}}";
  p.periodSeconds = 900;
  StatsParser sp2("sensor.t", p);
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp2, b, 5));
  Series s = sp2.series();
  TEST_ASSERT_TRUE(s.columns[0].has);
  TEST_ASSERT_EQUAL_FLOAT(7, s.columns[0].min);
  TEST_ASSERT_EQUAL_FLOAT(7, s.columns[0].max);
  TEST_ASSERT_FALSE(s.columns[1].has);  // end defaulted to start + 900 s = 1 column
}

void test_empty_and_other_entities() {
  StatsParams p = lineParams(24, 24);
  {
    StatsParser sp("sensor.t", p);
    TEST_ASSERT_EQUAL((int)StatsOutcome::NoStatistics,
                      (int)run(sp, "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{}}", 10));
  }
  {
    StatsParser sp("sensor.t", p);
    TEST_ASSERT_EQUAL((int)StatsOutcome::NoStatistics,
                      (int)run(sp, "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"sensor.t\":[]}}", 10));
  }
  {  // only another entity present
    StatsParser sp("sensor.t", p);
    TEST_ASSERT_EQUAL((int)StatsOutcome::NoStatistics, (int)run(sp, body("sensor.other", 3, 3600, false), 10));
  }
}

void test_malformed_and_truncated() {
  StatsParams p = lineParams(24, 24);
  {
    StatsParser sp("sensor.t", p);
    TEST_ASSERT_EQUAL((int)StatsOutcome::Malformed, (int)run(sp, "<html>nope</html>", 4));
  }
  {
    StatsParser sp("sensor.t", p);
    TEST_ASSERT_EQUAL((int)StatsOutcome::Malformed, (int)run(sp, "[1,2]", 4));
  }
  {
    StatsParser sp("sensor.t", p);
    TEST_ASSERT_EQUAL((int)StatsOutcome::Malformed, (int)run(sp, "", 4));
  }
  {
    StatsParser sp("sensor.t", p);
    std::string b = body("sensor.t", 5, 3600, false);
    TEST_ASSERT_EQUAL((int)StatsOutcome::Truncated, (int)run(sp, b.substr(0, b.size() - 20), 16));
  }
}

void test_bars() {
  StatsParams p;
  p.bars = true;
  p.statistic = StatsType::Change;
  p.layout.start = kT0;
  p.layout.end = kT0 + 6 * 86400;
  p.layout.bucketSeconds = 86400;
  p.layout.bucketCount = 6;
  StatsParser sp("sensor.t", p);
  // days 0..5, day 2 has no change value
  std::string b = body("sensor.t", 6, 86400, true, 2);
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, b, 50));
  Series s = sp.series();
  TEST_ASSERT_EQUAL(6, (int)s.buckets.size());
  TEST_ASSERT_EQUAL(86400, (int)s.bucketSeconds);
  TEST_ASSERT_EQUAL(5, (int)s.points);
  TEST_ASSERT_TRUE(s.buckets[1].has);
  TEST_ASSERT_EQUAL_FLOAT(0.25f, s.buckets[1].last);
  TEST_ASSERT_EQUAL_FLOAT(0.25f, s.buckets[1].mean);
  TEST_ASSERT_FALSE(s.buckets[2].has);
  float ch;
  TEST_ASSERT_TRUE(s.change(1, ch));
  TEST_ASSERT_FALSE(s.change(3, ch));  // predecessor has no data
}

void test_bars_mean_statistic_and_window() {
  StatsParams p;
  p.bars = true;
  p.statistic = StatsType::Mean;
  p.layout.start = kT0 + 3 * 3600;  // window starts at hour 3; hours 0..2 are outside
  p.layout.end = kT0 + 8 * 3600;
  p.layout.bucketSeconds = 3600;
  p.layout.bucketCount = 5;
  StatsParser sp("sensor.t", p);
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, body("sensor.t", 12, 3600, false), 128));
  Series s = sp.series();
  TEST_ASSERT_EQUAL(5, (int)s.points);
  TEST_ASSERT_EQUAL_FLOAT(13.5f, s.buckets[0].mean);
  TEST_ASSERT_EQUAL_FLOAT(17.5f, s.buckets[4].mean);
}

// Realistic HA day statistics in Europe/Budapest (UTC+1): buckets start at local midnight; window from barWindow.
void test_bars_local_midnight_days() {
  const int64_t now = 1761850000;  // 2025-10-30 ~18:46 UTC
  const int32_t off = 3600;
  const history::BarWindow w = barWindow(now, 168, 86400, off);
  TEST_ASSERT_EQUAL(7, w.count);
  TEST_ASSERT_EQUAL(0, (int)((w.start + off) % 86400));
  StatsParams p;
  p.bars = true;
  p.statistic = StatsType::Mean;
  p.layout.start = w.start;
  p.layout.end = now;
  p.layout.bucketSeconds = 86400;
  p.layout.bucketCount = w.count;
  p.periodSeconds = 86400;
  StatsParser sp("sensor.dp_buffer", p);
  std::string b = "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"sensor.dp_buffer\":[";
  for (int i = 4; i < 7; i++) {  // the last three days only
    int64_t st = (w.start + i * 86400) * 1000;
    if (i > 4) b += ",";
    b += "{\"start\":" + std::to_string(st) + ",\"end\":" + std::to_string(st + 86400000) + ",\"mean\":" +
         std::to_string(10 + i) + ".25}";
  }
  b += "]}}";
  TEST_ASSERT_EQUAL((int)StatsOutcome::Ok, (int)run(sp, b, 64));
  Series s = sp.series();
  TEST_ASSERT_EQUAL(3, (int)s.points);
  TEST_ASSERT_FALSE(s.buckets[3].has);
  TEST_ASSERT_TRUE(s.buckets[4].has);
  TEST_ASSERT_TRUE(s.buckets[6].has);
  TEST_ASSERT_EQUAL_FLOAT(16.25f, s.buckets[6].last);
  // x labels
  TEST_ASSERT_EQUAL_STRING("Today", barLabel(w.start + 6 * 86400, 86400, true, off).c_str());
  TEST_ASSERT_EQUAL_STRING("Tue", barLabel(w.start + 4 * 86400, 86400, false, off).c_str());  // 2025-10-28
  TEST_ASSERT_EQUAL_STRING("19", barLabel(1761850000 - 1761850000 % 3600, 3600, false, off).c_str());
}

// Bar charts are empty by their buckets, not by Series::range / columns (3 buckets arrived, no columns).
void test_bar_content_decision() {
  history::Series s;
  s.bucketSeconds = 86400;
  s.buckets.resize(7);
  TEST_ASSERT_FALSE(history::hasContent(history::ChartType::Bar, s));
  s.buckets[4].has = true; s.buckets[4].last = 12;
  s.buckets[6].has = true; s.buckets[6].last = 15.5f;
  s.points = 2;
  TEST_ASSERT_TRUE(history::hasContent(history::ChartType::Bar, s));
  float lo = 0, hi = 0;
  TEST_ASSERT_TRUE(history::barRange(s, lo, hi));
  TEST_ASSERT_EQUAL_FLOAT(12, lo);
  TEST_ASSERT_EQUAL_FLOAT(15.5f, hi);
  history::Series e;
  TEST_ASSERT_FALSE(history::hasContent(history::ChartType::Line, e));
}

void test_plans() {
  TEST_ASSERT_EQUAL((int)StatsPeriod::FiveMinute, (int)linePeriod(24));
  TEST_ASSERT_EQUAL((int)StatsPeriod::Hour, (int)linePeriod(25));
  TEST_ASSERT_EQUAL((int)StatsPeriod::Hour, (int)linePeriod(168));
  TEST_ASSERT_EQUAL((int)StatsPeriod::Day, (int)linePeriod(169));
  TEST_ASSERT_EQUAL((int)StatsPeriod::Hour, (int)barPeriod(BarPeriodCfg::Auto, 48));
  TEST_ASSERT_EQUAL((int)StatsPeriod::Day, (int)barPeriod(BarPeriodCfg::Auto, 49));
  TEST_ASSERT_EQUAL((int)StatsPeriod::Day, (int)barPeriod(BarPeriodCfg::Day, 24));
  TEST_ASSERT_EQUAL((int)StatsPeriod::Hour, (int)barPeriod(BarPeriodCfg::Hour, 720));
  TEST_ASSERT_EQUAL((int)Source::History, (int)chooseSource(SourceCfg::Auto, false, false, 24));
  TEST_ASSERT_EQUAL((int)Source::Statistics, (int)chooseSource(SourceCfg::Auto, false, false, 25));
  TEST_ASSERT_EQUAL((int)Source::History, (int)chooseSource(SourceCfg::Auto, true, false, 720));
  TEST_ASSERT_EQUAL((int)Source::Statistics, (int)chooseSource(SourceCfg::Auto, false, true, 1));
  TEST_ASSERT_EQUAL((int)Source::Statistics, (int)chooseSource(SourceCfg::History, false, true, 1));
  TEST_ASSERT_EQUAL((int)Source::Statistics, (int)chooseSource(SourceCfg::Statistics, false, false, 1));
  TEST_ASSERT_EQUAL((int)Source::History, (int)chooseSource(SourceCfg::History, false, false, 720));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_line_24h_5min_ms_and_iso);
  RUN_TEST(test_line_7d_hour_spans_columns);
  RUN_TEST(test_line_30d_day_fewer_columns_than_buckets);
  RUN_TEST(test_line_missing_values_and_gap);
  RUN_TEST(test_line_mean_only_falls_back);
  RUN_TEST(test_empty_and_other_entities);
  RUN_TEST(test_malformed_and_truncated);
  RUN_TEST(test_bars);
  RUN_TEST(test_bars_mean_statistic_and_window);
  RUN_TEST(test_bars_local_midnight_days);
  RUN_TEST(test_bar_content_decision);
  RUN_TEST(test_plans);
  RUN_TEST(test_huge_numbers_are_ignored);
  return UNITY_END();
}
