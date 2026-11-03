#include <unity.h>

#include "tab_clean.h"

using namespace screen_track;

void setUp() {}
void tearDown() {}

void test_plans() {
    TEST_ASSERT_FALSE(tabCleanPlan(0).init);
    TEST_ASSERT_TRUE(tabCleanPlan(0).full == FullWave::GC16);
    TEST_ASSERT_TRUE(tabCleanPlan(1).init);
    TEST_ASSERT_TRUE(tabCleanPlan(1).full == FullWave::GC16);
    TEST_ASSERT_TRUE(tabCleanPlan(2).init);
    TEST_ASSERT_TRUE(tabCleanPlan(2).full == FullWave::GL16);
    TEST_ASSERT_FALSE(tabCleanPlan(3).init);
    TEST_ASSERT_TRUE(tabCleanPlan(3).full == FullWave::GL16);
}

void test_invalid_is_current_behavior() {
    TEST_ASSERT_FALSE(tabCleanValid(-1));
    TEST_ASSERT_FALSE(tabCleanValid(4));
    TEST_ASSERT_TRUE(tabCleanValid(3));
    TEST_ASSERT_FALSE(tabCleanPlan(9).init);
    TEST_ASSERT_TRUE(tabCleanPlan(-1).full == FullWave::GC16);
}

void test_tab_change_only() {
    TEST_ASSERT_TRUE(isTabChange(true, false, false, 0, 1));
    TEST_ASSERT_FALSE(isTabChange(true, false, false, 1, 1));   // same tab (page change, settings)
    TEST_ASSERT_FALSE(isTabChange(false, false, false, 0, 1));  // partial frame (tap)
    TEST_ASSERT_FALSE(isTabChange(true, true, false, 0, 1));    // popup close
    TEST_ASSERT_FALSE(isTabChange(true, false, true, 0, 1));    // unknown panel: white fill path
    TEST_ASSERT_FALSE(isTabChange(true, false, false, -1, 1));  // first frame
    TEST_ASSERT_FALSE(isTabChange(true, false, false, 0, -1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_plans);
    RUN_TEST(test_invalid_is_current_behavior);
    RUN_TEST(test_tab_change_only);
    return UNITY_END();
}
