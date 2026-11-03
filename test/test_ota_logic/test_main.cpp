#include <unity.h>

#include "ota_logic.h"

using namespace ota_logic;

void setUp() {}
void tearDown() {}

static const char *kSha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789ABCDEF";

static std::string mj(const std::string &ver, const std::string &file, const char *size) {
  std::string s = "{";
  if (!ver.empty()) s += "\"version\":\"" + ver + "\",";
  s += "\"ota\":{\"file\":\"" + file + "\"," + (*size ? std::string("\"size\":") + size + "," : "") + "\"sha256\":\"" + kSha + "\"}}";
  return s;
}

void test_semver() {
  TEST_ASSERT_TRUE(isNewer("1.9.0", "1.10.0"));
  TEST_ASSERT_FALSE(isNewer("1.10.0", "1.9.9"));
  TEST_ASSERT_FALSE(isNewer("1.2.1", "1.2.1"));
  TEST_ASSERT_TRUE(isNewer("1.2.1", "v1.2.2"));
  TEST_ASSERT_TRUE(isNewer("1.2.1", "2.0"));
  TEST_ASSERT_FALSE(isNewer("1.2.1", "1.3.0-rc1"));  // pre-release ignored
  TEST_ASSERT_FALSE(isNewer("1.2.1", "garbage"));
  TEST_ASSERT_FALSE(isNewer("1.2.1", ""));
  Version v;
  TEST_ASSERT_FALSE(parseVersion("1.2.x", v));
  TEST_ASSERT_TRUE(parseVersion("1.3.0-rc1", v));
  TEST_ASSERT_TRUE(v.pre);
}

void test_schedule() {
  const int64_t t = 1800000000;
  CheckState s;
  TEST_ASSERT_TRUE(checkDue(s, t, true, true));    // cold boot
  TEST_ASSERT_FALSE(checkDue(s, t, true, false));  // never without Wi-Fi
  TEST_ASSERT_TRUE(checkDue(s, t, false, true));   // never checked
  TEST_ASSERT_FALSE(checkDue(s, 0, false, true));  // clock unset
  s.lastSuccess = s.lastAttempt = t;
  TEST_ASSERT_FALSE(checkDue(s, t + 6 * 86400, false, true));
  TEST_ASSERT_TRUE(checkDue(s, t + 7 * 86400, false, true));
  TEST_ASSERT_FALSE(checkDue(s, t + 7 * 86400, false, false));
  // after a failure at day 7: retry only after 1 day
  s.lastAttempt = t + 7 * 86400;
  TEST_ASSERT_FALSE(checkDue(s, t + 7 * 86400 + 3600, false, true));
  TEST_ASSERT_TRUE(checkDue(s, t + 8 * 86400, false, true));
}

void test_manifest() {
  Manifest m;
  TEST_ASSERT_TRUE(parseManifest(mj("1.3.0", "firmware.bin", "1000"), m));
  TEST_ASSERT_EQUAL_STRING("1.3.0", m.version.c_str());
  TEST_ASSERT_EQUAL_UINT32(1000, m.size);
  TEST_ASSERT_EQUAL_CHAR('a', m.sha256[10]);
  TEST_ASSERT_EQUAL_CHAR('f', m.sha256[63]);  // lower-cased
  TEST_ASSERT_TRUE(offer("1.2.1", m, 2000));
  TEST_ASSERT_FALSE(offer("1.3.0", m, 2000));
  TEST_ASSERT_FALSE(offer("1.2.1", m, 500));  // too big for the slot
  TEST_ASSERT_FALSE(parseManifest("{\"version\":\"1.3.0\"}", m));
  TEST_ASSERT_FALSE(parseManifest("not json", m));
  TEST_ASSERT_FALSE(parseManifest("{\"version\":\"1.3.0\",\"ota\":{\"file\":\"f\",\"size\":5,\"sha256\":\"abc\"}}", m));
  TEST_ASSERT_FALSE(parseManifest(mj("", "f", "5"), m));        // no version
  TEST_ASSERT_FALSE(parseManifest(mj("1.3.0", "../x", "5"), m));  // path trick
  TEST_ASSERT_FALSE(parseManifest(mj("1.3.0", "f", ""), m));      // no size
  TEST_ASSERT_FALSE(parseManifest(mj("x.y", "f", "5"), m));       // bad version
}

void test_install_gate() {
  TEST_ASSERT_TRUE(installAllowed(true, 10) == InstallBlock::None);
  TEST_ASSERT_TRUE(installAllowed(false, 50) == InstallBlock::None);
  TEST_ASSERT_TRUE(installAllowed(false, 49) == InstallBlock::Battery);
}

void test_rollback() {
  TEST_ASSERT_FALSE(rollbackNow(1));
  TEST_ASSERT_FALSE(rollbackNow(2));
  TEST_ASSERT_TRUE(rollbackNow(3));  // 3 earlier unhealthy boots
  // healthy = first frame drawn + 10 s in loop(); network / HA state is not an input
  TEST_ASSERT_FALSE(healthy(true, 9999));
  TEST_ASSERT_TRUE(healthy(true, 10000));
  TEST_ASSERT_FALSE(healthy(false, 60000));
}

void test_cold_boot_hourly_cap() {
  const int64_t t = 1800000000;
  CheckState s;
  s.lastAttempt = t;
  TEST_ASSERT_FALSE(checkDue(s, t + 3599, true, true));  // reboot loop: not again within the hour
  TEST_ASSERT_TRUE(checkDue(s, t + 3600, true, true));
  TEST_ASSERT_FALSE(checkDue(s, t + 3600, true, false));
  TEST_ASSERT_TRUE(checkDue(s, 0, true, true));  // clock unset: cannot tell, the cold boot checks
}

void test_suppressed() {
  Manifest m;
  TEST_ASSERT_TRUE(parseManifest(mj("1.3.0", "firmware.bin", "1000"), m));
  TEST_ASSERT_TRUE(offer("1.2.1", m, 2000));   // no suppression: a rolled-back version is offered again
}

void test_limits() {
  TEST_ASSERT_TRUE(manifestLenOk(-1));  // chunked: the reader caps it
  TEST_ASSERT_TRUE(manifestLenOk(8192));
  TEST_ASSERT_FALSE(manifestLenOk(8193));
  TEST_ASSERT_FALSE(memoryOk(40 * 1024 - 1));
  TEST_ASSERT_TRUE(memoryOk(40 * 1024));
}

void test_job_deadline() {
  TEST_ASSERT_FALSE(jobExpired(false, 0));
  TEST_ASSERT_FALSE(jobExpired(false, 59999));
  TEST_ASSERT_TRUE(jobExpired(false, 60000));
  TEST_ASSERT_TRUE(jobExpired(false, 61000));
  TEST_ASSERT_FALSE(jobExpired(true, 60000));  // install has the longer cap
  TEST_ASSERT_FALSE(jobExpired(true, 299999));
  TEST_ASSERT_TRUE(jobExpired(true, 300000));
  TEST_ASSERT_TRUE(jobExpired(true, 400000));
}

// Task 098 (090): deadline as ota.cpp::cancelled() evaluates it: elapsed = millis() - start, unsigned.
void test_098_deadline_scenarios() {
  TEST_ASSERT_EQUAL_UINT32(60000, kCheckDeadlineMs);
  TEST_ASSERT_EQUAL_UINT32(300000, kInstallDeadlineMs);
  // slow install: still running at 4:59, "Took too long" path (cancelled() true) at 5:00
  const uint32_t start = 1000;
  TEST_ASSERT_FALSE(jobExpired(true, (start + 299999u) - start));
  TEST_ASSERT_TRUE(jobExpired(true, (start + 300000u) - start));
  // manifest stall: a check dies at 60 s although the install cap would still allow it
  TEST_ASSERT_TRUE(jobExpired(false, 60000));
  TEST_ASSERT_FALSE(jobExpired(true, 60000));
  // millis() wrap in the middle of a job
  const uint32_t nearWrap = 0xFFFFFF00u;
  TEST_ASSERT_FALSE(jobExpired(false, (uint32_t)(nearWrap + 59000u) - nearWrap));
  TEST_ASSERT_TRUE(jobExpired(false, (uint32_t)(nearWrap + 60000u) - nearWrap));
}

// Task 098 (087): battery gate and low-memory skip edges.
void test_098_battery_gate_and_memory() {
  TEST_ASSERT_TRUE(installAllowed(false, 0) == InstallBlock::Battery);
  TEST_ASSERT_TRUE(installAllowed(false, 49) == InstallBlock::Battery);
  TEST_ASSERT_TRUE(installAllowed(false, 50) == InstallBlock::None);
  TEST_ASSERT_TRUE(installAllowed(false, 100) == InstallBlock::None);
  TEST_ASSERT_TRUE(installAllowed(true, 0) == InstallBlock::None);  // USB wins at any level
  TEST_ASSERT_FALSE(memoryOk(0));
  TEST_ASSERT_FALSE(memoryOk(kMinFreeBlock - 1));
  TEST_ASSERT_TRUE(memoryOk(kMinFreeBlock));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_semver);
  RUN_TEST(test_schedule);
  RUN_TEST(test_manifest);
  RUN_TEST(test_install_gate);
  RUN_TEST(test_rollback);
  RUN_TEST(test_cold_boot_hourly_cap);
  RUN_TEST(test_suppressed);
  RUN_TEST(test_limits);
  RUN_TEST(test_job_deadline);
  RUN_TEST(test_098_deadline_scenarios);
  RUN_TEST(test_098_battery_gate_and_memory);
  return UNITY_END();
}
