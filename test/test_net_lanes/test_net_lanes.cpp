#include <unity.h>

#include "net_lanes.h"
#include "ota_logic.h"

using namespace app_logic;

void setUp() {}
void tearDown() {}

static NetCommand mk(NetCommand::Type t, bool user = false) {
  NetCommand c;
  c.type = t;
  c.user = user;
  return c;
}
using T = NetCommand::Type;

static void test_lanes() {
  TEST_ASSERT_EQUAL(Lane::User, laneOf(mk(T::Call)));
  TEST_ASSERT_EQUAL(Lane::User, laneOf(mk(T::NotifDismiss)));
  TEST_ASSERT_EQUAL(Lane::User, laneOf(mk(T::Fetch, true)));
  TEST_ASSERT_EQUAL(Lane::Background, laneOf(mk(T::Fetch)));
  TEST_ASSERT_EQUAL(Lane::Background, laneOf(mk(T::History)));
  TEST_ASSERT_EQUAL(Lane::Background, laneOf(mk(T::Report)));
  TEST_ASSERT_EQUAL(Lane::Background, laneOf(mk(T::Notifications)));
  TEST_ASSERT_EQUAL(Lane::Background, laneOf(mk(T::Forecast)));
  TEST_ASSERT_EQUAL(Lane::Background, laneOf(mk(T::Calendar)));
  TEST_ASSERT_EQUAL(Lane::Background, laneOf(mk(T::OtaCheck)));  // the automatic OTA check never outranks a tap
}

static void test_tap_behind_three_background_jobs_starts_next() {
  LaneQueue<int> q;
  q.push(Lane::Background, 1);
  q.push(Lane::Background, 2);
  q.push(Lane::Background, 3);
  TEST_ASSERT_FALSE(q.userWaiting());
  q.push(Lane::User, 99);
  TEST_ASSERT_TRUE(q.userWaiting());
  int v = 0;
  TEST_ASSERT_TRUE(q.pop(v));
  TEST_ASSERT_EQUAL(99, v);
  TEST_ASSERT_FALSE(q.userWaiting());
  for (int e = 1; e <= 3; e++) {
    TEST_ASSERT_TRUE(q.pop(v));
    TEST_ASSERT_EQUAL(e, v);  // background stays FIFO
  }
  TEST_ASSERT_FALSE(q.pop(v));
}

static void test_user_lane_fifo_and_requeue_front() {
  LaneQueue<int> q;
  q.push(Lane::Background, 1);
  q.push(Lane::User, 10);
  q.push(Lane::User, 11);
  q.requeueFront(0);  // a yielded job keeps its place ahead of the other background jobs
  int v;
  q.pop(v); TEST_ASSERT_EQUAL(10, v);
  q.pop(v); TEST_ASSERT_EQUAL(11, v);
  q.pop(v); TEST_ASSERT_EQUAL(0, v);
  q.pop(v); TEST_ASSERT_EQUAL(1, v);
  TEST_ASSERT_EQUAL(0, (int)q.size());
}

static void test_yield_policy() {
  TEST_ASSERT_EQUAL(YieldPolicy::Requeue, yieldPolicyOf(T::History));
  TEST_ASSERT_EQUAL(YieldPolicy::Requeue, yieldPolicyOf(T::Notifications));
  TEST_ASSERT_EQUAL(YieldPolicy::Requeue, yieldPolicyOf(T::Forecast));
  TEST_ASSERT_EQUAL(YieldPolicy::Requeue, yieldPolicyOf(T::Calendar));
  TEST_ASSERT_EQUAL(YieldPolicy::Drop, yieldPolicyOf(T::Report));
  TEST_ASSERT_EQUAL(YieldPolicy::None, yieldPolicyOf(T::Call));
  TEST_ASSERT_EQUAL(YieldPolicy::None, yieldPolicyOf(T::Fetch));
}

static void test_ota_refused_when_interactive() {
  TEST_ASSERT_FALSE(ota_logic::autoCheckAllowed(true, false, 5000, false));   // user active
  TEST_ASSERT_FALSE(ota_logic::autoCheckAllowed(true, false, 59999, false));
  TEST_ASSERT_FALSE(ota_logic::autoCheckAllowed(true, false, 90000, true));   // idle but user lane busy
  TEST_ASSERT_TRUE(ota_logic::autoCheckAllowed(true, false, 60000, false));   // always-awake, idle 60 s
  TEST_ASSERT_TRUE(ota_logic::autoCheckAllowed(false, false, 0, false));      // scheduled wake
  TEST_ASSERT_TRUE(ota_logic::autoCheckAllowed(true, true, 0, true));         // Settings > Check now
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_lanes);
  RUN_TEST(test_tap_behind_three_background_jobs_starts_next);
  RUN_TEST(test_user_lane_fifo_and_requeue_front);
  RUN_TEST(test_yield_policy);
  RUN_TEST(test_ota_refused_when_interactive);
  return UNITY_END();
}
