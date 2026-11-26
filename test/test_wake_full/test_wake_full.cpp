#include <unity.h>

#include "wake_full.h"

using namespace screen_track;

void setUp() {}
void tearDown() {}

void test_scheduled_invalid_record_one_full() {
    const auto p = invalidRecordPlan(true);
    TEST_ASSERT_FALSE(p.fullNow);
    TEST_ASSERT_TRUE(p.deferChanged);
    TEST_ASSERT_TRUE(p.whiteFirst);
    TEST_ASSERT_EQUAL(1, fullsPerWake(p, true));
}

void test_interactive_invalid_record_full_now() {
    const auto p = invalidRecordPlan(false);
    TEST_ASSERT_TRUE(p.fullNow);
    TEST_ASSERT_FALSE(p.whiteFirst);
    TEST_ASSERT_EQUAL(1, fullsPerWake(p, false));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_scheduled_invalid_record_one_full);
    RUN_TEST(test_interactive_invalid_record_full_now);
    return UNITY_END();
}
