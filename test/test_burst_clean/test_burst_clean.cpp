#include <unity.h>

#include "burst_clean.h"

using namespace screen_track;

void setUp() {}
void tearDown() {}

static BurstTracker after(int partials) {
    BurstTracker t;
    for (int i = 0; i < partials; ++i) t.onPartial();
    return t;
}

void test_counter_threshold_is_more_than_n() {
    const BurstConfig c;  // n = 4, 2 s
    const BurstContext x;
    TEST_ASSERT_FALSE(after(4).due(c, 5000, x));
    TEST_ASSERT_TRUE(after(5).due(c, 5000, x));
    TEST_ASSERT_FALSE(after(0).due(c, 5000, x));
}

void test_idle_timer() {
    const BurstConfig c;
    const BurstContext x;
    const BurstTracker t = after(10);
    TEST_ASSERT_FALSE(t.due(c, 1999, x));
    TEST_ASSERT_TRUE(t.due(c, 2000, x));
}

void test_exclusions() {
    const BurstConfig c;
    const BurstTracker t = after(10);
    BurstContext x;
    x.popup = true;
    TEST_ASSERT_FALSE(t.due(c, 9999, x));
    x = BurstContext{};
    x.settings = true;
    TEST_ASSERT_FALSE(t.due(c, 9999, x));
    x = BurstContext{};
    x.ota = true;
    TEST_ASSERT_FALSE(t.due(c, 9999, x));
    x = BurstContext{};
    x.tap_pending = true;
    TEST_ASSERT_FALSE(t.due(c, 9999, x));
    x = BurstContext{};
    x.scheduled = true;
    TEST_ASSERT_FALSE(t.due(c, 9999, x));
    TEST_ASSERT_TRUE(t.due(c, 9999, BurstContext{}));
}

void test_any_full_resets() {
    BurstTracker t = after(10);
    t.onFull();
    TEST_ASSERT_EQUAL_INT(0, t.partials());
    TEST_ASSERT_FALSE(t.due(BurstConfig{}, 9999, BurstContext{}));
    t.onPartial();
    TEST_ASSERT_EQUAL_INT(1, t.partials());
}

void test_off_and_custom_n() {
    BurstConfig c;
    c.n = -1;
    TEST_ASSERT_FALSE(after(50).due(c, 9999, BurstContext{}));
    c.n = 0;  // every partial triggers
    TEST_ASSERT_TRUE(after(1).due(c, 9999, BurstContext{}));
    c.n = 9;
    TEST_ASSERT_FALSE(after(9).due(c, 9999, BurstContext{}));
    TEST_ASSERT_TRUE(after(10).due(c, 9999, BurstContext{}));
}

void test_modes_map_to_plans() {
    TEST_ASSERT_FALSE(burstPlan(BurstMode::GC16).init);
    TEST_ASSERT_TRUE(burstPlan(BurstMode::GC16).full == FullWave::GC16);
    TEST_ASSERT_FALSE(burstPlan(BurstMode::GL16).init);
    TEST_ASSERT_TRUE(burstPlan(BurstMode::GL16).full == FullWave::GL16);
    TEST_ASSERT_TRUE(burstPlan(BurstMode::InitGL16).init);
    TEST_ASSERT_TRUE(burstPlan(BurstMode::InitGL16).full == FullWave::GL16);
}

void test_parse_mode() {
    BurstMode m = BurstMode::GC16;
    TEST_ASSERT_TRUE(parseBurstMode("init+gl16", m));
    TEST_ASSERT_TRUE(m == BurstMode::InitGL16);
    TEST_ASSERT_TRUE(parseBurstMode("gl16", m));
    TEST_ASSERT_TRUE(m == BurstMode::GL16);
    TEST_ASSERT_FALSE(parseBurstMode("gl16x", m));
    TEST_ASSERT_FALSE(parseBurstMode("", m));
    TEST_ASSERT_TRUE(m == BurstMode::GL16);
    TEST_ASSERT_EQUAL_STRING("init+gl16", burstModeName(BurstMode::InitGL16));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_counter_threshold_is_more_than_n);
    RUN_TEST(test_idle_timer);
    RUN_TEST(test_exclusions);
    RUN_TEST(test_any_full_resets);
    RUN_TEST(test_off_and_custom_n);
    RUN_TEST(test_modes_map_to_plans);
    RUN_TEST(test_parse_mode);
    return UNITY_END();
}
