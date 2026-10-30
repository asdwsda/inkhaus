#include <unity.h>

#include "modal_settle.h"

using namespace screen_track;

void setUp() {}
void tearDown() {}

void test_not_due_without_change() {
    ModalSettle m(1000);
    TEST_ASSERT_FALSE(m.due(5000));
}

void test_due_after_quiet_and_resets() {
    ModalSettle m(1000);
    m.noteFastest(Rect{10, 20, 30, 40}, 100);
    TEST_ASSERT_FALSE(m.due(1099));
    TEST_ASSERT_TRUE(m.due(1100));
    m.done();
    TEST_ASSERT_FALSE(m.due(9000));
}

void test_new_change_restarts_timer_and_unions() {
    ModalSettle m(1000);
    m.noteFastest(Rect{10, 20, 30, 40}, 100);
    m.noteFastest(Rect{50, 10, 20, 20}, 900);
    TEST_ASSERT_FALSE(m.due(1500));
    TEST_ASSERT_TRUE(m.due(1900));
    TEST_ASSERT_EQUAL(10, m.rect().x);
    TEST_ASSERT_EQUAL(10, m.rect().y);
    TEST_ASSERT_EQUAL(60, m.rect().w);
    TEST_ASSERT_EQUAL(50, m.rect().h);
}

void test_rect_restarts_after_done_and_clock_wrap() {
    ModalSettle m(1000);
    m.noteFastest(Rect{0, 0, 10, 10}, 0xFFFFFF00u);
    TEST_ASSERT_TRUE(m.due(0x300u));
    m.done();
    m.noteFastest(Rect{100, 100, 5, 5}, 2000);
    TEST_ASSERT_EQUAL(100, m.rect().x);
    TEST_ASSERT_EQUAL(5, m.rect().w);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_not_due_without_change);
    RUN_TEST(test_due_after_quiet_and_resets);
    RUN_TEST(test_new_change_restarts_timer_and_unions);
    RUN_TEST(test_rect_restarts_after_done_and_clock_wrap);
    return UNITY_END();
}
