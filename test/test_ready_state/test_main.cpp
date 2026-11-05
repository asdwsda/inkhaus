// Task 120: ready indicator state, labels, refresh budget, ghost-debt rule.
#include <unity.h>

#include <ArduinoJson.h>
#include "config_model.h"  // first include of the config lib: keeps the PlatformIO LDF order that resolves its ArduinoJson dependency

#include "ready_state.h"
#include "wall_time.h"

using namespace app_logic;

void setUp() {}
void tearDown() {}

static ReadyInputs wake() {
  ReadyInputs i;
  i.interactive = true;
  return i;
}

void test_scheduled_and_demo_draw_nothing() {
  ReadyTracker t;
  ReadyInputs i;  // not interactive
  TEST_ASSERT_TRUE(t.step(0, i).shown == ReadyShown::None);
  i.wifiUp = true;
  TEST_ASSERT_TRUE(t.step(1000, i).shown == ReadyShown::None);
  i = wake();
  i.demo = true;
  TEST_ASSERT_TRUE(t.step(2000, i).shown == ReadyShown::None);
  TEST_ASSERT_EQUAL(0, t.changes());
}

void test_slow_network_three_partials() {
  ReadyTracker t;
  ReadyInputs i = wake();
  TEST_ASSERT_TRUE(t.step(0, i).shown == ReadyShown::NotReady);  // drawn at once at wake
  i.wifiUp = true;
  TEST_ASSERT_TRUE(t.step(1000, i).shown == ReadyShown::NotReady);  // ready held for the coalescing time
  TEST_ASSERT_TRUE(t.step(1000 + kReadyCoalesceMs, i).shown == ReadyShown::Ready);
  i.fetchDone = true;
  TEST_ASSERT_TRUE(t.step(3000, i).shown == ReadyShown::None);  // `updated` = classic status
  TEST_ASSERT_EQUAL(3, t.changes());  // wake, ready, updated
}

void test_fast_network_two_partials() {
  ReadyTracker t;
  ReadyInputs i = wake();
  t.step(0, i);
  i.wifiUp = true;
  t.step(500, i);
  t.step(700, i);  // still inside the 400 ms hold
  i.fetchDone = true;
  TEST_ASSERT_TRUE(t.step(800, i).shown == ReadyShown::None);  // straight to updated
  TEST_ASSERT_EQUAL(2, t.changes());
}

void test_queued_tap_costs_one_status_partial_once() {
  ReadyTracker t;
  ReadyInputs i = wake();
  t.step(0, i);
  i.queuedTaps = 1;
  ReadyView v = t.step(100, i);
  TEST_ASSERT_TRUE(v.shown == ReadyShown::NotReady && v.queued);
  TEST_ASSERT_EQUAL(2, t.changes());
  i.queuedTaps = 3;  // further taps: no status change
  t.step(200, i);
  TEST_ASSERT_EQUAL(2, t.changes());
  i.wifiUp = true;  // link ready: queued flag drops with the state change, one partial
  t.step(300, i);
  t.step(300 + kReadyCoalesceMs, i);
  TEST_ASSERT_TRUE(t.view().shown == ReadyShown::Ready && !t.view().queued);
  TEST_ASSERT_EQUAL(3, t.changes());
}

void test_offline_and_recovery() {
  ReadyTracker t;
  ReadyInputs i = wake();
  t.step(0, i);
  i.offline = true;
  TEST_ASSERT_TRUE(t.step(100, i).shown == ReadyShown::Offline);
  i.queuedTaps = 2;
  TEST_ASSERT_TRUE(t.step(200, i).queued);
  i.offline = false;  // new session: connecting again
  i.queuedTaps = 0;
  i.wifiUp = false;
  TEST_ASSERT_TRUE(t.step(300, i).shown == ReadyShown::NotReady);
}

void test_lost_after_wake_is_offline_then_classic() {
  ReadyTracker t;
  ReadyInputs i = wake();
  i.wifiUp = true;
  i.fetchDone = true;
  TEST_ASSERT_TRUE(t.step(0, i).shown == ReadyShown::None);  // wake over: classic status incl. `fetching states`
  i.offline = true;
  TEST_ASSERT_TRUE(t.step(10, i).shown == ReadyShown::Offline);
  i.offline = false;
  TEST_ASSERT_TRUE(t.step(20, i).shown == ReadyShown::None);
}

void test_labels() {
  const int64_t now = 1767225600 + 14 * 3600 + 5 * 60;
  const int64_t ok = now - 60;
  Label l = statusLabel(Phase::NotReady, ok, now, 0, 60, false, false);
  TEST_ASSERT_EQUAL_STRING("not ready, connecting", l.line1.c_str());
  TEST_ASSERT_FALSE(l.stale);
  l = statusLabel(Phase::NotReady, ok, now, 0, 60, false, true);
  TEST_ASSERT_EQUAL_STRING("not ready", l.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("connecting", l.line2.c_str());
  l = statusLabel(Phase::NotReady, ok, now, 0, 60, false, false, true);
  TEST_ASSERT_EQUAL_STRING("queued, connecting", l.line1.c_str());
  l = statusLabel(Phase::NotReady, ok, now, 0, 60, false, true, true);
  TEST_ASSERT_EQUAL_STRING("queued", l.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("connecting", l.line2.c_str());
  l = statusLabel(Phase::Ready, ok, now, 0, 60, false, true);
  TEST_ASSERT_EQUAL_STRING("ready", l.line1.c_str());
  TEST_ASSERT_TRUE(l.line2.empty());
  l = statusLabel(Phase::Offline, ok, now, 0, 60, true, false, true);
  TEST_ASSERT_EQUAL_STRING("offline, queued", l.line1.c_str());
  TEST_ASSERT_TRUE(l.stale);  // clock icon stays
  l = statusLabel(Phase::Offline, ok, now, 0, 60, true, true, true);
  TEST_ASSERT_EQUAL_STRING("queued", l.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("offline", l.line2.c_str());
  l = statusLabel(Phase::Offline, ok, now, 0, 60, true, false);  // unchanged without a queue
  TEST_ASSERT_EQUAL_STRING("offline, last 14:04", l.line1.c_str());
}

void test_ghost_by_area_only() {
  const IndicatorRect status{0, 0, 412, 54};  // portrait wake rect
  TEST_ASSERT_TRUE(countsByAreaOnly({0, 0, 288, 54}, status));
  TEST_ASSERT_TRUE(countsByAreaOnly({0, 0, 412, 54}, status));
  TEST_ASSERT_FALSE(countsByAreaOnly({0, 60, 288, 54}, status));  // a widget rect: a feedback refresh
  TEST_ASSERT_FALSE(countsByAreaOnly({0, 0, 540, 54}, status));   // spills out
  TEST_ASSERT_FALSE(countsByAreaOnly({0, 0, 0, 0}, status));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_scheduled_and_demo_draw_nothing);
  RUN_TEST(test_slow_network_three_partials);
  RUN_TEST(test_fast_network_two_partials);
  RUN_TEST(test_queued_tap_costs_one_status_partial_once);
  RUN_TEST(test_offline_and_recovery);
  RUN_TEST(test_lost_after_wake_is_offline_then_classic);
  RUN_TEST(test_labels);
  RUN_TEST(test_ghost_by_area_only);
  return UNITY_END();
}
