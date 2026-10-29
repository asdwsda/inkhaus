#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include "axis.h"
#include "cache.h"
#include "collector.h"
#include "iso8601.h"
#include "scanner.h"

using namespace history;

void setUp() {}
void tearDown() {}

namespace {
const int64_t kT0 = 1761696000;  // 2025-10-29T00:00:00Z

std::string iso(int64_t t) {
  int64_t days = t / 86400, rem = t % 86400;
  // civil from days
  int64_t z = days + 719468;
  int64_t era = z / 146097, doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int64_t mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) ++y;
  char b[48];
  snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02d.123456+00:00", (int)y, (int)m, (int)d, (int)(rem / 3600),
           (int)(rem / 60 % 60), (int)(rem % 60));
  return b;
}

// Body for one entity: points every `stepS` seconds, value from fn(i).
template <class F>
std::string body(int64_t start, int64_t end, int64_t stepS, F fn) {
  std::string s = "[[{\"entity_id\":\"sensor.x\",\"state\":\"" + fn(0) + "\",\"attributes\":{},\"last_changed\":\"" +
                  iso(start) + "\",\"last_updated\":\"" + iso(start) + "\"}";
  int i = 1;
  for (int64_t t = start + stepS; t < end; t += stepS, ++i)
    s += ",{\"last_changed\":\"" + iso(t) + "\",\"state\":\"" + fn(i) + "\"}";
  return s + "]]";
}

std::string num(int i) {
  char b[16];
  snprintf(b, sizeof b, "%.1f", 20.0 + (i % 50) * 0.1);
  return b;
}

struct Run {
  Series s;
  ScanResult r;
  size_t points;
};

Run run(const std::string& b, const Params& p, size_t chunk) {
  Collector c(p);
  Scanner sc(c);
  for (size_t i = 0; i < b.size(); i += chunk) sc.feed(b.data() + i, std::min(chunk, b.size() - i));
  ScanResult r = sc.finish();
  return {c.finish(), r, sc.points()};
}
Params P(int64_t hours, int cols) {
  Params p;
  p.start = kT0;
  p.end = kT0 + hours * 3600;
  p.columns = cols;
  return p;
}
}  // namespace

void test_iso() {
  int64_t e = 0;
  TEST_ASSERT_TRUE(parseIso8601("2025-10-29T00:00:00+00:00", 25, e));
  TEST_ASSERT_EQUAL_INT64(kT0, e);
  std::string a = "1970-01-01T00:00:00Z";
  TEST_ASSERT_TRUE(parseIso8601(a.data(), a.size(), e));
  TEST_ASSERT_EQUAL_INT64(0, e);
  a = "2024-02-29T23:59:59.999999+00:00";
  TEST_ASSERT_TRUE(parseIso8601(a.data(), a.size(), e));
  TEST_ASSERT_EQUAL_INT64(1709251199, e);
  a = "2025-10-29T02:00:00+02:00";
  TEST_ASSERT_TRUE(parseIso8601(a.data(), a.size(), e));
  TEST_ASSERT_EQUAL_INT64(kT0, e);
  a = "2025-10-28T18:30:00-0530";
  TEST_ASSERT_TRUE(parseIso8601(a.data(), a.size(), e));
  TEST_ASSERT_EQUAL_INT64(kT0, e);
  for (const char* bad : {"", "2025-10-29", "2025-13-29T00:00:00Z", "2025-10-29T00:00:00+0", "abc", "2025-10-29T00:00:00Zx",
                          "2025-10-29T00:00:00."})
    TEST_ASSERT_FALSE_MESSAGE(parseIso8601(bad, strlen(bad), e), bad);
  TEST_ASSERT_EQUAL_INT64(kT0, e);  // untouched
}

void test_fixture_24h_every_chunk_size() {
  std::string b = body(kT0, kT0 + 86400, 180, num);  // 480 points
  Run ref = run(b, P(24, 600), b.size());
  TEST_ASSERT_EQUAL(ScanResult::Ok, ref.r);
  TEST_ASSERT_EQUAL(480, ref.points);
  TEST_ASSERT_EQUAL(Kind::Numeric, ref.s.kind);
  TEST_ASSERT_EQUAL(600, ref.s.columns.size());
  for (const auto& c : ref.s.columns) TEST_ASSERT_TRUE(c.has);
  float lo, hi;
  TEST_ASSERT_TRUE(ref.s.range(lo, hi));
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 20.0, lo);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 24.9, hi);
  for (size_t chunk : {1u, 2u, 3u, 7u, 64u, 1000u}) {
    Run r = run(b, P(24, 600), chunk);
    TEST_ASSERT_EQUAL(ScanResult::Ok, r.r);
    TEST_ASSERT_EQUAL(480, r.points);
    for (size_t i = 0; i < 600; ++i) {
      TEST_ASSERT_EQUAL_FLOAT(ref.s.columns[i].min, r.s.columns[i].min);
      TEST_ASSERT_EQUAL_FLOAT(ref.s.columns[i].max, r.s.columns[i].max);
      TEST_ASSERT_EQUAL_FLOAT(ref.s.columns[i].last, r.s.columns[i].last);
    }
  }
}

// Split the body into exactly two chunks at every byte boundary.
void test_split_at_every_boundary() {
  std::string b = body(kT0, kT0 + 3 * 3600, 600, [](int i) { return i == 3 ? std::string("unavailable") : num(i); });
  Run ref = run(b, P(3, 90), b.size());
  for (size_t cut = 0; cut <= b.size(); ++cut) {
    Collector c(P(3, 90));
    Scanner sc(c);
    sc.feed(b.data(), cut);
    sc.feed(b.data() + cut, b.size() - cut);
    TEST_ASSERT_EQUAL(ScanResult::Ok, sc.finish());
    TEST_ASSERT_EQUAL(ref.points, sc.points());
    Series s = c.finish();
    for (size_t i = 0; i < 90; ++i) TEST_ASSERT_EQUAL_FLOAT(ref.s.columns[i].last, s.columns[i].last);
  }
}

void test_7d_fixture() {
  std::string b = body(kT0, kT0 + 7 * 86400, 120, num);  // 5040 points
  Run r = run(b, P(7 * 24, 700), 1460);
  TEST_ASSERT_EQUAL(ScanResult::Ok, r.r);
  TEST_ASSERT_EQUAL(5040, r.points);
  TEST_ASSERT_EQUAL(700, r.s.columns.size());
  for (const auto& c : r.s.columns) TEST_ASSERT_TRUE(c.has);
}

void test_empty_bodies() {
  for (const char* b : {"[]", "[[]]", " [ [ ] ] \n"}) {
    Run r = run(b, P(24, 100), 1);
    TEST_ASSERT_EQUAL_MESSAGE(ScanResult::Ok, r.r, b);
    TEST_ASSERT_EQUAL(0, r.points);
    TEST_ASSERT_TRUE(r.s.empty());
    TEST_ASSERT_EQUAL(Kind::Unknown, r.s.kind);
    for (const auto& c : r.s.columns) TEST_ASSERT_FALSE(c.has);
  }
}

void test_malformed_and_truncated() {
  Run r = run("{\"message\":\"401: Unauthorized\"}", P(24, 10), 5);
  TEST_ASSERT_EQUAL(ScanResult::Malformed, r.r);
  r = run("<html><body>502</body></html>", P(24, 10), 5);
  TEST_ASSERT_EQUAL(ScanResult::Malformed, r.r);
  r = run("", P(24, 10), 5);
  TEST_ASSERT_EQUAL(ScanResult::Malformed, r.r);
  std::string b = body(kT0, kT0 + 3600, 60, num);
  r = run(b.substr(0, b.size() / 2), P(1, 60), 13);
  TEST_ASSERT_EQUAL(ScanResult::Truncated, r.r);
  TEST_ASSERT_TRUE(r.points > 10);
  // columns after the last entry carry the last value forward
  TEST_ASSERT_TRUE(r.s.columns.back().has);
}

void test_unavailable_unknown_skipped() {
  std::string b = "[[{\"entity_id\":\"sensor.x\",\"state\":\"unavailable\",\"last_changed\":\"" + iso(kT0) +
                  "\"},{\"last_changed\":\"" + iso(kT0 + 600) + "\",\"state\":\"20.0\"},{\"last_changed\":\"" +
                  iso(kT0 + 1200) + "\",\"state\":\"unknown\"},{\"last_changed\":\"" + iso(kT0 + 1800) +
                  "\",\"state\":\"unavailable\"},{\"last_changed\":\"" + iso(kT0 + 2400) + "\",\"state\":\"22.0\"}]]";
  Run r = run(b, P(1, 6), 4);
  TEST_ASSERT_EQUAL(ScanResult::Ok, r.r);
  TEST_ASSERT_EQUAL(5, r.points);  // scanner delivers all entries
  TEST_ASSERT_EQUAL(2, r.s.points);  // collector accepts the two numeric ones
  TEST_ASSERT_EQUAL(Kind::Numeric, r.s.kind);
  TEST_ASSERT_FALSE(r.s.columns[0].has);  // before the first valid entry
  TEST_ASSERT_TRUE(r.s.columns[1].has);
  TEST_ASSERT_EQUAL_FLOAT(20.f, r.s.columns[3].last);  // carried over the unavailable gap
  TEST_ASSERT_EQUAL_FLOAT(22.f, r.s.columns[4].last);
  TEST_ASSERT_EQUAL_FLOAT(20.f, r.s.columns[4].min);   // column 4 starts at the carried 20
  TEST_ASSERT_EQUAL_FLOAT(22.f, r.s.columns[5].last);
}

void test_carry_forward_and_clamping() {
  // 10 columns of 360 s. Entry before start (-> col 0), one at 1000 s, one after end (ignored).
  std::string b = "[[{\"state\":\"5\",\"last_changed\":\"" + iso(kT0 - 500) + "\"},{\"last_changed\":\"" + iso(kT0 + 1000) +
                  "\",\"state\":\"9\"},{\"last_changed\":\"" + iso(kT0 + 4000) + "\",\"state\":\"99\"}]]";
  Run r = run(b, P(1, 10), 100);
  TEST_ASSERT_EQUAL_FLOAT(5.f, r.s.columns[0].last);
  TEST_ASSERT_EQUAL_FLOAT(5.f, r.s.columns[1].last);
  TEST_ASSERT_EQUAL_FLOAT(5.f, r.s.columns[2].min);  // 1000 s = col 2: starts at 5, changes to 9
  TEST_ASSERT_EQUAL_FLOAT(9.f, r.s.columns[2].max);
  TEST_ASSERT_EQUAL_FLOAT(9.f, r.s.columns[2].last);
  TEST_ASSERT_EQUAL_FLOAT(9.f, r.s.columns[9].last);
  TEST_ASSERT_EQUAL(2, r.s.points);
}

void test_binary() {
  std::string b = "[[{\"state\":\"off\",\"last_changed\":\"" + iso(kT0) + "\"},{\"last_changed\":\"" + iso(kT0 + 1000) +
                  "\",\"state\":\"on\"},{\"last_changed\":\"" + iso(kT0 + 2000) + "\",\"state\":\"off\"},{\"last_changed\":\"" +
                  iso(kT0 + 2100) + "\",\"state\":\"unavailable\"},{\"last_changed\":\"" + iso(kT0 + 3000) +
                  "\",\"state\":\"23.5\"}]]";
  Run r = run(b, P(1, 10), 3);
  TEST_ASSERT_EQUAL(Kind::Binary, r.s.kind);
  TEST_ASSERT_EQUAL(3, r.s.points);  // 23.5 is the wrong kind
  TEST_ASSERT_EQUAL_FLOAT(0.f, r.s.columns[1].last);
  TEST_ASSERT_EQUAL_FLOAT(1.f, r.s.columns[2].last);
  TEST_ASSERT_EQUAL_FLOAT(1.f, r.s.columns[3].last);
  TEST_ASSERT_EQUAL_FLOAT(1.f, r.s.columns[5].max);
  TEST_ASSERT_EQUAL_FLOAT(0.f, r.s.columns[6].last);
  TEST_ASSERT_EQUAL_FLOAT(0.f, r.s.columns[9].last);
}

void test_unsupported() {
  std::string b = "[[{\"state\":\"heat\",\"last_changed\":\"" + iso(kT0) + "\"},{\"last_changed\":\"" + iso(kT0 + 10) +
                  "\",\"state\":\"21\"}]]";
  Run r = run(b, P(1, 10), 7);
  TEST_ASSERT_EQUAL(Kind::Unsupported, r.s.kind);
  TEST_ASSERT_TRUE(r.s.empty());
}

void test_long_strings_and_nested_attributes() {
  // attributes (nested object, long strings) in the first entry must not confuse the scanner
  std::string longv(500, 'x');
  std::string b = "[[{\"entity_id\":\"sensor.x\",\"state\":\"7.5\",\"attributes\":{\"friendly_name\":\"" + longv +
                  "\",\"state\":\"bogus\",\"nested\":{\"state\":\"3\"}},\"last_changed\":\"" + iso(kT0) +
                  "\"},{\"last_changed\":\"" + iso(kT0 + 60) + "\",\"state\":\"8 \\\"q\\\"\"},{\"last_changed\":\"" +
                  iso(kT0 + 120) + "\",\"state\":\"9\"}]]";
  Run r = run(b, P(1, 4), 1);
  TEST_ASSERT_EQUAL(ScanResult::Ok, r.r);
  TEST_ASSERT_EQUAL(3, r.points);
  TEST_ASSERT_EQUAL(2, r.s.points);  // "8 "q"" is not a number
  TEST_ASSERT_EQUAL_FLOAT(9.f, r.s.columns[3].last);
}

void test_buckets() {
  // 3 buckets of 1 h; value 10 for first 30 min, 20 for the next 90 min, then 30.
  std::string b = "[[{\"state\":\"10\",\"last_changed\":\"" + iso(kT0) + "\"},{\"last_changed\":\"" + iso(kT0 + 1800) +
                  "\",\"state\":\"20\"},{\"last_changed\":\"" + iso(kT0 + 2 * 3600 + 1800) + "\",\"state\":\"30\"}]]";
  Params p = P(3, 30);
  p.bucketSeconds = 3600;
  p.bucketCount = 3;
  Run r = run(b, p, 2);
  TEST_ASSERT_EQUAL(3, r.s.buckets.size());
  TEST_ASSERT_EQUAL_FLOAT(15.f, r.s.buckets[0].mean);
  TEST_ASSERT_EQUAL_FLOAT(10.f, r.s.buckets[0].min);
  TEST_ASSERT_EQUAL_FLOAT(20.f, r.s.buckets[0].max);
  TEST_ASSERT_EQUAL_FLOAT(20.f, r.s.buckets[0].last);
  TEST_ASSERT_EQUAL_FLOAT(20.f, r.s.buckets[1].mean);
  TEST_ASSERT_EQUAL_FLOAT(25.f, r.s.buckets[2].mean);
  TEST_ASSERT_EQUAL_FLOAT(30.f, r.s.buckets[2].last);
  float ch;
  TEST_ASSERT_TRUE(r.s.change(1, ch));
  TEST_ASSERT_EQUAL_FLOAT(0.f, ch);
  TEST_ASSERT_TRUE(r.s.change(2, ch));
  TEST_ASSERT_EQUAL_FLOAT(10.f, ch);
}

void test_bucket_gap() {
  // data only in the last bucket: earlier buckets have no data
  std::string b = "[[{\"state\":\"4\",\"last_changed\":\"" + iso(kT0 + 2 * 3600 + 5) + "\"}]]";
  Params p = P(3, 6);
  p.bucketSeconds = 3600;
  p.bucketCount = 3;
  Run r = run(b, p, 100);
  TEST_ASSERT_FALSE(r.s.buckets[0].has);
  TEST_ASSERT_FALSE(r.s.buckets[1].has);
  TEST_ASSERT_TRUE(r.s.buckets[2].has);
  float ch;
  TEST_ASSERT_FALSE(r.s.change(1, ch));
}

void test_axis() {
  Axis a = makeAxis(20.0, 25.0, false, 0, false, 0);
  TEST_ASSERT_FLOAT_WITHIN(1e-5, 19.75, (float)(a.lo));
  TEST_ASSERT_FLOAT_WITHIN(1e-5, 25.25, (float)(a.hi));
  TEST_ASSERT_TRUE(a.ticks.size() >= 2 && a.ticks.size() <= 3);
  for (double t : a.ticks) TEST_ASSERT_TRUE(t >= a.lo && t <= a.hi);
  TEST_ASSERT_FLOAT_WITHIN(1e-5, 0.0, (float)(fmod(a.ticks[0], a.step)));
  a = makeAxis(21.0, 21.0, false, 0, false, 0);  // flat
  TEST_ASSERT_TRUE(a.hi > a.lo);
  TEST_ASSERT_TRUE(a.ticks.size() >= 1);
  a = makeAxis(3, 9, true, 0, true, 100);  // fixed
  TEST_ASSERT_EQUAL_FLOAT(0, (float)(a.lo));
  TEST_ASSERT_EQUAL_FLOAT(100, (float)(a.hi));
  TEST_ASSERT_EQUAL(3, a.ticks.size());
  TEST_ASSERT_EQUAL_FLOAT(50, (float)(a.step));
  TEST_ASSERT_EQUAL_FLOAT(0.5, (float)(a.frac(50)));
  TEST_ASSERT_EQUAL_FLOAT(1.0, (float)(a.frac(500)));
  a = makeAxis(3, 9, true, 50, true, 10);  // inverted fixed bounds ignored
  TEST_ASSERT_TRUE(a.lo < 3 && a.hi > 9);
  a = makeAxis(0.0, 0.3, false, 0, false, 0);
  TEST_ASSERT_TRUE(a.decimals >= 1);
  a = makeAxis(0, 1000, false, 0, false, 0, 5);
  TEST_ASSERT_TRUE(a.ticks.size() <= 5);
  TEST_ASSERT_EQUAL(0, a.decimals);
  a = makeAxis(-5, 5, false, 0, false, 0);
  bool zero = false;
  for (double t : a.ticks) zero |= (t == 0);
  TEST_ASSERT_TRUE(zero);
}

void test_cache_roundtrip() {
  std::string b = body(kT0, kT0 + 86400, 300, num);
  Params p = P(24, 700);
  p.bucketSeconds = 3600;
  p.bucketCount = 24;
  Run r = run(b, p, 100);
  CacheParams cp;
  cp.entity = "sensor.x";
  cp.columns = 700;
  uint32_t h = paramsHash(cp);
  auto bytes = serialize(r.s, h, 1761700000);
  TEST_ASSERT_TRUE(bytes.size() < 700 * 6 + 24 * 17 + 80);
  Series o;
  int64_t fa = 0;
  TEST_ASSERT_EQUAL(CacheStatus::Ok, deserialize(bytes.data(), bytes.size(), h, o, fa));
  TEST_ASSERT_EQUAL_INT64(1761700000, fa);
  TEST_ASSERT_EQUAL(r.s.kind, o.kind);
  TEST_ASSERT_EQUAL_INT64(r.s.start, o.start);
  TEST_ASSERT_EQUAL(700, o.columns.size());
  TEST_ASSERT_EQUAL(24, o.buckets.size());
  for (size_t i = 0; i < 700; ++i) {
    TEST_ASSERT_FLOAT_WITHIN(1e-3, r.s.columns[i].min, o.columns[i].min);
    TEST_ASSERT_FLOAT_WITHIN(1e-3, r.s.columns[i].max, o.columns[i].max);
    TEST_ASSERT_FLOAT_WITHIN(1e-3, r.s.columns[i].last, o.columns[i].last);
    TEST_ASSERT_EQUAL(r.s.columns[i].has, o.columns[i].has);
  }
  TEST_ASSERT_EQUAL_FLOAT(r.s.buckets[5].mean, o.buckets[5].mean);
  // wrong params, corruption, truncation
  TEST_ASSERT_EQUAL(CacheStatus::WrongParams, deserialize(bytes.data(), bytes.size(), h + 1, o, fa));
  auto bad = bytes;
  bad[100] ^= 0x55;
  TEST_ASSERT_EQUAL(CacheStatus::Corrupt, deserialize(bad.data(), bad.size(), h, o, fa));
  TEST_ASSERT_EQUAL(CacheStatus::Corrupt, deserialize(bytes.data(), bytes.size() - 1, h, o, fa));
  TEST_ASSERT_EQUAL(CacheStatus::Corrupt, deserialize(bytes.data(), 3, h, o, fa));
}

void test_cache_gaps_and_empty() {
  Run r = run("[[]]", P(24, 50), 100);
  auto bytes = serialize(r.s, 7, 5);
  Series o;
  int64_t fa;
  TEST_ASSERT_EQUAL(CacheStatus::Ok, deserialize(bytes.data(), bytes.size(), 7, o, fa));
  TEST_ASSERT_TRUE(o.empty());
  TEST_ASSERT_FALSE(o.columns[3].has);
}

void test_params_hash() {
  CacheParams a;
  a.entity = "sensor.x";
  a.columns = 700;
  uint32_t h = paramsHash(a);
  TEST_ASSERT_EQUAL_UINT32(h, paramsHash(a));
  CacheParams b = a;
  b.hours = 168;
  TEST_ASSERT_NOT_EQUAL(h, paramsHash(b));
  b = a; b.entity = "sensor.y";
  TEST_ASSERT_NOT_EQUAL(h, paramsHash(b));
  b = a; b.hasMax = true; b.max = 30;
  TEST_ASSERT_NOT_EQUAL(h, paramsHash(b));
  b = a; b.columns = 480;
  TEST_ASSERT_NOT_EQUAL(h, paramsHash(b));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_iso);
  RUN_TEST(test_fixture_24h_every_chunk_size);
  RUN_TEST(test_split_at_every_boundary);
  RUN_TEST(test_7d_fixture);
  RUN_TEST(test_empty_bodies);
  RUN_TEST(test_malformed_and_truncated);
  RUN_TEST(test_unavailable_unknown_skipped);
  RUN_TEST(test_carry_forward_and_clamping);
  RUN_TEST(test_binary);
  RUN_TEST(test_unsupported);
  RUN_TEST(test_long_strings_and_nested_attributes);
  RUN_TEST(test_buckets);
  RUN_TEST(test_bucket_gap);
  RUN_TEST(test_axis);
  RUN_TEST(test_cache_roundtrip);
  RUN_TEST(test_cache_gaps_and_empty);
  RUN_TEST(test_params_hash);
  return UNITY_END();
}
