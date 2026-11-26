#include <unity.h>

#include "config_source.h"

using namespace config;

void setUp() {}
void tearDown() {}

void test_cache_match_used() {
  TEST_ASSERT_EQUAL(int(SourceAction::UseCache), int(decideSource(SdStat::Ok, true, true)));
}
void test_changed_file_reimports() {
  TEST_ASSERT_EQUAL(int(SourceAction::ReadSd), int(decideSource(SdStat::Ok, true, false)));
  TEST_ASSERT_EQUAL(int(SourceAction::ReadSd), int(decideSource(SdStat::Ok, false, false)));
}
void test_unreadable_keeps_cache() {
  TEST_ASSERT_EQUAL(int(SourceAction::KeepCache), int(decideSource(SdStat::NoCard, true, false)));
  TEST_ASSERT_EQUAL(int(SourceAction::KeepCache), int(decideSource(SdStat::NoFile, true, false)));
}
void test_never_configured_is_demo() {
  TEST_ASSERT_EQUAL(int(SourceAction::Demo), int(decideSource(SdStat::NoCard, false, false)));
  TEST_ASSERT_EQUAL(int(SourceAction::Demo), int(decideSource(SdStat::NoFile, false, false)));
}
void test_problem_reads_sd() {
  TEST_ASSERT_EQUAL(int(SourceAction::ReadSd), int(decideSource(SdStat::Problem, true, false)));
}
void test_retry() {
  int n = 0, pauses = 0;
  TEST_ASSERT_TRUE(retry(3, [&] { return ++n == 3; }, [&] { ++pauses; }));
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(2, pauses);
  n = pauses = 0;
  TEST_ASSERT_FALSE(retry(3, [&] { ++n; return false; }, [&] { ++pauses; }));
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(2, pauses);
  n = 0;
  TEST_ASSERT_TRUE(retry(3, [&] { ++n; return true; }, [] {}));
  TEST_ASSERT_EQUAL(1, n);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cache_match_used);
  RUN_TEST(test_changed_file_reimports);
  RUN_TEST(test_unreadable_keeps_cache);
  RUN_TEST(test_never_configured_is_demo);
  RUN_TEST(test_problem_reads_sd);
  RUN_TEST(test_retry);
  return UNITY_END();
}
