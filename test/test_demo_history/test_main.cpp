#include <unity.h>

#include "demo_history.h"

using namespace history;

void setUp() {}
void tearDown() {}

static DemoParams line(const char* id) {
  DemoParams p;
  p.entity = id; p.end = 1793361600; p.start = p.end - 24 * 3600; p.columns = 392;
  return p;
}

void test_deterministic() {
  const Series a = demoSeries(line("sensor.living_room_temperature"));
  const Series b = demoSeries(line("sensor.living_room_temperature"));
  const Series c = demoSeries(line("sensor.kitchen_temperature"));
  TEST_ASSERT_EQUAL(392, (int)a.columns.size());
  bool differs = false;
  for (size_t i = 0; i < a.columns.size(); ++i) {
    TEST_ASSERT_EQUAL_FLOAT(a.columns[i].last, b.columns[i].last);
    if (a.columns[i].last != c.columns[i].last) differs = true;
  }
  TEST_ASSERT_TRUE(differs);
}

void test_in_range_and_complete() {
  const char* ids[] = {"sensor.t_temperature", "sensor.hall_humidity", "sensor.washer_power", "sensor.phone_battery", "sensor.x"};
  for (const char* id : ids) {
    const Series s = demoSeries(line(id));
    float lo, hi;
    demoRange(id, lo, hi);
    TEST_ASSERT_EQUAL((int)Kind::Numeric, (int)s.kind);
    TEST_ASSERT_EQUAL(392, (int)s.points);
    for (const Column& c : s.columns) {
      TEST_ASSERT_TRUE(c.has);
      TEST_ASSERT_TRUE(c.min >= lo && c.max <= hi && c.min <= c.last && c.last <= c.max);
    }
    float a, b;
    TEST_ASSERT_TRUE(s.range(a, b));
    TEST_ASSERT_TRUE(b > a);  // not a flat line
  }
}

void test_binary_steps() {
  DemoParams p = line("binary_sensor.front_door");
  p.binary = true; p.columns = 764;
  const Series s = demoSeries(p);
  TEST_ASSERT_EQUAL((int)Kind::Binary, (int)s.kind);
  int on = 0, off = 0;
  for (const Column& c : s.columns) {
    TEST_ASSERT_TRUE(c.min == 0 || c.min == 1);
    TEST_ASSERT_TRUE(c.max == 0 || c.max == 1);
    (c.last > 0.5f ? on : off)++;
  }
  TEST_ASSERT_TRUE(on > 0 && off > 0);
}

void test_bars() {
  DemoParams p = line("sensor.grid_energy");
  p.bar = true; p.bucketSeconds = 3600; p.bucketCount = 24; p.start = p.end - 24 * 3600;
  const Series s = demoSeries(p);
  TEST_ASSERT_EQUAL(24, (int)s.buckets.size());
  TEST_ASSERT_EQUAL(3600, (int)s.bucketSeconds);
  for (size_t i = 0; i < s.buckets.size(); ++i) {
    TEST_ASSERT_TRUE(s.buckets[i].has);
    float ch;
    if (i > 0) { TEST_ASSERT_TRUE(s.change(i, ch)); TEST_ASSERT_TRUE(ch > 0); }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_deterministic);
  RUN_TEST(test_in_range_and_complete);
  RUN_TEST(test_binary_steps);
  RUN_TEST(test_bars);
  return UNITY_END();
}
