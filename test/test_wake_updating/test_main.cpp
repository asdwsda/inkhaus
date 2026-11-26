// Task 127: wake "not fresh yet" look: state machine, refresh budget, end rule, grey mapping.
#include <unity.h>

#include <ArduinoJson.h>
#include "config_model.h"  // first include of the config lib: keeps the PlatformIO LDF order that resolves its ArduinoJson dependency

#include "wake_updating.h"

using namespace app_logic;

void setUp() {}
void tearDown() {}

static ReadyInputs wake() {
  ReadyInputs i;
  i.interactive = true;
  return i;
}

void test_scheduled_and_demo_never_stale() {
  WakeUpdating w;
  ReadyInputs i;  // scheduled (not interactive)
  TEST_ASSERT_FALSE(w.step(i));
  i.wifiUp = true;
  TEST_ASSERT_FALSE(w.step(i));
  i = wake();
  i.demo = true;
  TEST_ASSERT_FALSE(w.step(i));
  TEST_ASSERT_EQUAL(0, w.lookRefreshes());
}

void test_stale_from_first_frame_until_first_fetch() {
  WakeUpdating w;
  ReadyInputs i = wake();
  TEST_ASSERT_TRUE(w.step(i));  // the wake frame already shows it
  i.wifiUp = true;
  TEST_ASSERT_TRUE(w.step(i));  // Wi-Fi up, still stale: states trickle in, nothing changes
  i.queuedTaps = 2;
  TEST_ASSERT_TRUE(w.step(i));
  TEST_ASSERT_TRUE(w.stale());
  i.fetchDone = true;
  TEST_ASSERT_FALSE(w.step(i));
  TEST_ASSERT_TRUE(w.ended() == StaleEnd::Fresh);
}

void test_refresh_budget_is_two_per_wake() {
  WakeUpdating w;
  ReadyInputs i = wake();
  for (int n = 0; n < 50; ++n) w.step(i);  // many loop passes while connecting: no extra refresh
  TEST_ASSERT_EQUAL(1, w.lookRefreshes());
  i.wifiUp = true;
  for (int n = 0; n < 50; ++n) w.step(i);
  TEST_ASSERT_EQUAL(1, w.lookRefreshes());
  i.fetchDone = true;
  for (int n = 0; n < 50; ++n) w.step(i);
  TEST_ASSERT_EQUAL(2, w.lookRefreshes());  // look partial + one refresh when fresh
}

void test_timeout_ends_the_look_once() {
  WakeUpdating w;
  ReadyInputs i = wake();
  w.step(i);
  i.offline = true;
  TEST_ASSERT_FALSE(w.step(i));
  TEST_ASSERT_TRUE(w.ended() == StaleEnd::Timeout);
  TEST_ASSERT_EQUAL(2, w.lookRefreshes());
}

void test_latched_after_end() {
  WakeUpdating w;
  ReadyInputs i = wake();
  w.step(i);
  i.fetchDone = true;
  w.step(i);
  i.fetchDone = false;  // e.g. a later manual refresh or a reconnect: the look never returns
  i.offline = true;
  TEST_ASSERT_FALSE(w.step(i));
  i.offline = false;
  TEST_ASSERT_FALSE(w.step(i));
  TEST_ASSERT_EQUAL(2, w.lookRefreshes());
  TEST_ASSERT_TRUE(w.ended() == StaleEnd::Fresh);
}

void test_no_look_when_already_offline_or_done() {
  WakeUpdating w;
  ReadyInputs i = wake();
  i.offline = true;
  TEST_ASSERT_FALSE(w.step(i));
  TEST_ASSERT_EQUAL(0, w.lookRefreshes());
  WakeUpdating v;
  i = wake();
  i.fetchDone = true;
  TEST_ASSERT_FALSE(v.step(i));
  TEST_ASSERT_EQUAL(0, v.lookRefreshes());
}

void test_end_full_rule() {
  TEST_ASSERT_FALSE(staleEndIsFull(0));
  TEST_ASSERT_FALSE(staleEndIsFull(6));
  TEST_ASSERT_TRUE(staleEndIsFull(7));
}

void test_grey_mapping() {
  TEST_ASSERT_EQUAL_UINT8(0x88, staleGrey(0));
  TEST_ASSERT_EQUAL_UINT8(255, staleGrey(255));
  const uint8_t mid = staleGrey(128);
  TEST_ASSERT_TRUE(mid > 0x88 && mid < 255);
  for (int v = 1; v < 256; ++v) TEST_ASSERT_TRUE(staleGrey(uint8_t(v)) >= staleGrey(uint8_t(v - 1)));  // monotonic
}

void test_failed_first_fetch_ends_as_timeout() {
  WakeUpdating w;
  ReadyInputs i = wake();
  w.step(i);
  i.fetchDone = true;
  TEST_ASSERT_FALSE(w.step(i, true));
  TEST_ASSERT_EQUAL(int(StaleEnd::Timeout), int(w.ended()));
  WakeUpdating ok;
  ok.step(wake());
  ok.step(i, false);
  TEST_ASSERT_EQUAL(int(StaleEnd::Fresh), int(ok.ended()));
}

void test_tapped_widget_excluded_for_the_wake() {
  WakeUpdating w;
  w.step(wake());
  TEST_ASSERT_FALSE(w.excluded(0, 3));
  w.exclude(0, 3);
  w.exclude(0, 3);
  TEST_ASSERT_TRUE(w.excluded(0, 3));
  TEST_ASSERT_FALSE(w.excluded(1, 3));
  TEST_ASSERT_FALSE(w.excluded(0, 2));
  ReadyInputs i = wake();
  i.fetchDone = true;
  w.step(i);
  TEST_ASSERT_TRUE(w.excluded(0, 3));  // stays out after the end too
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_scheduled_and_demo_never_stale);
  RUN_TEST(test_stale_from_first_frame_until_first_fetch);
  RUN_TEST(test_refresh_budget_is_two_per_wake);
  RUN_TEST(test_timeout_ends_the_look_once);
  RUN_TEST(test_latched_after_end);
  RUN_TEST(test_no_look_when_already_offline_or_done);
  RUN_TEST(test_end_full_rule);
  RUN_TEST(test_grey_mapping);
  RUN_TEST(test_failed_first_fetch_ends_as_timeout);
  RUN_TEST(test_tapped_widget_excluded_for_the_wake);
  return UNITY_END();
}
