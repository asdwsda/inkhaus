// Task 124a: shared glue state of calendar/forecast/art (post first due, changed flag, Cancelled handling).
#include <unity.h>

#include "cal_events.h"
#include "forecast.h"
#include "widget_glue.h"

void setUp() {}
void tearDown() {}

template <class Store>
static void check_post_and_cancel() {
  Store store;
  widget_glue::FetchState fs;
  const std::vector<std::string> ents = {"a.one", "a.two"};
  std::vector<std::string> posted;
  auto post = [&](const std::string &e) { posted.push_back(e); };

  TEST_ASSERT_TRUE(widget_glue::postFirstDue(fs, store, ents, 1000, true, post));
  TEST_ASSERT_TRUE(fs.pending);
  TEST_ASSERT_EQUAL_STRING("a.one", fs.asking.c_str());
  // answered with Cancelled: forgotten, so the same entity is due again
  TEST_ASSERT_TRUE(widget_glue::finishStore(fs, store, true));
  TEST_ASSERT_FALSE(fs.pending);
  TEST_ASSERT_TRUE(store.due("a.one", 1000, true));
  TEST_ASSERT_TRUE(widget_glue::postFirstDue(fs, store, ents, 1000, true, post));
  TEST_ASSERT_EQUAL_STRING("a.one", posted.back().c_str());
  // answered normally (or failed): stays asked, the next entity follows
  TEST_ASSERT_FALSE(widget_glue::finishStore(fs, store, false));
  TEST_ASSERT_FALSE(store.due("a.one", 1000, true));
  TEST_ASSERT_TRUE(widget_glue::postFirstDue(fs, store, ents, 1000, true, post));
  TEST_ASSERT_EQUAL_STRING("a.two", fs.asking.c_str());
  TEST_ASSERT_EQUAL(3, (int)posted.size());
  // nothing due: no post, pending untouched
  widget_glue::finishStore(fs, store, false);
  TEST_ASSERT_FALSE(widget_glue::postFirstDue(fs, store, ents, 1000, true, post));
  TEST_ASSERT_FALSE(fs.pending);
}

static void test_calendar_store() { check_post_and_cancel<calendar::Store>(); }
static void test_forecast_store() { check_post_and_cancel<forecast::Store>(); }

static void test_flags() {
  widget_glue::FetchState fs;
  fs.pending = true;
  fs.changed = true;
  fs.newSession();
  TEST_ASSERT_FALSE(fs.pending);
  TEST_ASSERT_TRUE(fs.takeChanged());
  TEST_ASSERT_FALSE(fs.takeChanged());
  fs.pending = true;
  TEST_ASSERT_TRUE(fs.finish(true));
  TEST_ASSERT_FALSE(fs.pending);
  TEST_ASSERT_FALSE(fs.finish(false));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_calendar_store);
  RUN_TEST(test_forecast_store);
  RUN_TEST(test_flags);
  return UNITY_END();
}
