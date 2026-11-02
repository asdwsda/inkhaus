#include <unity.h>

#include "bounded_wait.h"

using screen_track::boundedWait;

void setUp() {}
void tearDown() {}

static void test_ready_at_once() {
    uint32_t t = 0;
    int steps = 0;
    TEST_ASSERT_TRUE(boundedWait([] { return true; }, [&] { return t; }, [&] { ++steps; }, 300));
    TEST_ASSERT_EQUAL(0, steps);
}

static void test_ready_after_steps() {
    uint32_t t = 0;
    int steps = 0;
    TEST_ASSERT_TRUE(boundedWait([&] { return steps >= 5; }, [&] { return t; }, [&] { ++steps; ++t; }, 300));
    TEST_ASSERT_EQUAL(5, steps);
}

static void test_times_out_and_feeds() {
    uint32_t t = 0;
    int feeds = 0;
    TEST_ASSERT_FALSE(boundedWait([] { return false; }, [&] { return t; }, [&] { ++feeds; ++t; }, 300));
    TEST_ASSERT_EQUAL(300, feeds);  // fed every poll, stopped at the bound
    TEST_ASSERT_EQUAL_UINT32(300, t);
}

static void test_clock_wrap() {
    uint32_t t = 0xFFFFFF00u;
    TEST_ASSERT_FALSE(boundedWait([] { return false; }, [&] { return t; }, [&] { t += 50; }, 300));
    TEST_ASSERT_TRUE(t - 0xFFFFFF00u >= 300);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ready_at_once);
    RUN_TEST(test_ready_after_steps);
    RUN_TEST(test_times_out_and_feeds);
    RUN_TEST(test_clock_wrap);
    return UNITY_END();
}
