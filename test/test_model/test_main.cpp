#include <unity.h>

#include "state_store.h"

void setUp() {}
void tearDown() {}

static EntityState make(const char* id, const char* state) {
  EntityState e;
  e.id = id;
  e.state = state;
  e.available = true;
  return e;
}

void test_update_reports_new_ids() {
  StateStore s;
  auto ch = s.update({make("light.a", "on"), make("cover.b", "open")});
  TEST_ASSERT_EQUAL(2, ch.size());
  TEST_ASSERT_EQUAL_STRING("light.a", ch[0].c_str());
  TEST_ASSERT_EQUAL_STRING("cover.b", ch[1].c_str());
  TEST_ASSERT_NOT_NULL(s.get("light.a"));
  TEST_ASSERT_NULL(s.get("nope.x"));
}

void test_update_reports_only_changed() {
  StateStore s;
  s.update({make("light.a", "on"), make("cover.b", "open"), make("sensor.c", "1")});
  EntityState c = make("cover.b", "closed");
  auto ch = s.update({make("light.a", "on"), c, make("sensor.c", "1")});
  TEST_ASSERT_EQUAL(1, ch.size());
  TEST_ASSERT_EQUAL_STRING("cover.b", ch[0].c_str());
  TEST_ASSERT_EQUAL_STRING("closed", s.get("cover.b")->state.c_str());
}

void test_update_attribute_and_availability_change() {
  StateStore s;
  EntityState a = make("climate.t", "heat");
  a.temperature = 21.0;
  s.update({a});
  a.temperature = 21.5;
  TEST_ASSERT_EQUAL(1, s.update({a}).size());
  a.available = false;
  TEST_ASSERT_EQUAL(1, s.update({a}).size());
  TEST_ASSERT_EQUAL(0, s.update({a}).size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_update_reports_new_ids);
  RUN_TEST(test_update_reports_only_changed);
  RUN_TEST(test_update_attribute_and_availability_change);
  return UNITY_END();
}
