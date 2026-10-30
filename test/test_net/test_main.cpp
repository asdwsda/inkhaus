#include <unity.h>

#include <cstddef>

#include "net_policy.h"
#include "wifi_select.h"

using namespace netpolicy;

static const uint8_t kB[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};

static CacheRecord rec(bool st = false) {
    return makeRecord("home", kB, 6, st, 1, 2, 3, 4);
}

void setUp() {}
void tearDown() {}

void test_default_record_invalid() {
    TEST_ASSERT_FALSE(cacheValid(CacheRecord{}, "home", false));
}
void test_valid_record() {
    TEST_ASSERT_TRUE(cacheValid(rec(), "home", false));
    TEST_ASSERT_TRUE(hasLease(rec()));
}
void test_wrong_ssid_invalid() {
    TEST_ASSERT_FALSE(cacheValid(rec(), "other", false));
}
void test_static_mode_mismatch_invalid() {
    TEST_ASSERT_FALSE(cacheValid(rec(false), "home", true));
    TEST_ASSERT_FALSE(cacheValid(rec(true), "home", false));
    TEST_ASSERT_TRUE(cacheValid(rec(true), "home", true));
}
void test_static_record_has_no_lease() {
    CacheRecord r = rec(true);
    TEST_ASSERT_FALSE(hasLease(r));
    TEST_ASSERT_EQUAL_UINT32(0, r.ip);
}
void test_bad_channel_or_bssid() {
    CacheRecord r = rec();
    r.channel = 0;
    TEST_ASSERT_FALSE(cacheValid(r, "home", false));
    r.channel = 15;
    TEST_ASSERT_FALSE(cacheValid(r, "home", false));
    const uint8_t zero[6] = {0};
    TEST_ASSERT_FALSE(cacheValid(makeRecord("home", zero, 6, false, 0, 0, 0, 0), "home", false));
}
void test_invalidated_after_two_failures() {
    CacheRecord r = rec();
    TEST_ASSERT_TRUE(cacheUsable(r, "home", false));
    r = afterCachedFailure(r);
    TEST_ASSERT_TRUE(cacheUsable(r, "home", false));
    r = afterCachedFailure(r);
    TEST_ASSERT_FALSE(cacheUsable(r, "home", false));
}

void test_cached_success_fast() {
    Connector c;
    TEST_ASSERT_EQUAL(Attempt::Cached, c.start(1000, true));
    Attempt n;
    TEST_ASSERT_EQUAL(Phase::Connecting, c.step(1100, LinkStatus::Connecting, n));
    TEST_ASSERT_EQUAL(Attempt::None, n);
    TEST_ASSERT_EQUAL(Phase::Connected, c.step(1300, LinkStatus::Connected, n));
    TEST_ASSERT_FALSE(c.cachedFailed());
}
void test_cached_timeout_falls_back_to_scan() {
    Connector c;
    c.start(0, true);
    Attempt n;
    c.step(kCachedAttemptMs - 1, LinkStatus::Connecting, n);
    TEST_ASSERT_EQUAL(Attempt::None, n);
    TEST_ASSERT_EQUAL(Phase::Connecting, c.step(kCachedAttemptMs, LinkStatus::Connecting, n));
    TEST_ASSERT_EQUAL(Attempt::Scan, n);
    TEST_ASSERT_TRUE(c.cachedFailed());
    TEST_ASSERT_EQUAL(Phase::Connected, c.step(4000, LinkStatus::Connected, n));
}
void test_cached_driver_failure_falls_back_early() {
    Connector c;
    c.start(0, true);
    Attempt n;
    c.step(500, LinkStatus::Failed, n);
    TEST_ASSERT_EQUAL(Attempt::Scan, n);
    // scan then fails at the driver level: no further restarts, wait for overall timeout
    TEST_ASSERT_EQUAL(Phase::Connecting, c.step(900, LinkStatus::Failed, n));
    TEST_ASSERT_EQUAL(Attempt::None, n);
}
void test_overall_timeout() {
    Connector c;
    c.start(100, false);
    Attempt n;
    TEST_ASSERT_EQUAL(Phase::Connecting, c.step(100 + kTotalTimeoutMs - 1, LinkStatus::Connecting, n));
    TEST_ASSERT_EQUAL(Phase::Failed, c.step(100 + kTotalTimeoutMs, LinkStatus::Connecting, n));
    TEST_ASSERT_FALSE(c.cachedFailed());
    // stays failed
    TEST_ASSERT_EQUAL(Phase::Failed, c.step(100 + kTotalTimeoutMs + 5, LinkStatus::Connected, n));
}
void test_scan_start_never_uses_cached() {
    Connector c;
    TEST_ASSERT_EQUAL(Attempt::Scan, c.start(0, false));
    Attempt n;
    c.step(kCachedAttemptMs + 10, LinkStatus::Connecting, n);
    TEST_ASSERT_EQUAL(Attempt::None, n);
}
void test_millis_wraparound() {
    Connector c;
    c.start(0xFFFFFF00u, true);
    Attempt n;
    c.step(0xFFFFFF00u + kCachedAttemptMs, LinkStatus::Connecting, n);
    TEST_ASSERT_EQUAL(Attempt::Scan, n);
}
void test_timeout_with_cached_active_counts_failure() {
    Connector c;
    c.start(0, true);
    Attempt n;
    c.step(kTotalTimeoutMs, LinkStatus::Connecting, n);
    TEST_ASSERT_TRUE(c.cachedFailed());
}

// ---- tester additions (task 021 review) ----
void test_cached_fallback_boundary() {
    Connector c;
    c.start(1000, true);
    Attempt n;
    c.step(1000 + kCachedAttemptMs - 1, LinkStatus::Connecting, n);
    TEST_ASSERT_EQUAL(Attempt::None, n);
    TEST_ASSERT_FALSE(c.cachedFailed());
    c.step(1000 + kCachedAttemptMs, LinkStatus::Connecting, n);
    TEST_ASSERT_EQUAL(Attempt::Scan, n);
}
void test_scan_driver_failure_waits_for_total_timeout() {
    Connector c;
    c.start(0, false);
    Attempt n;
    TEST_ASSERT_EQUAL(Phase::Connecting, c.step(100, LinkStatus::Failed, n));
    TEST_ASSERT_EQUAL(Attempt::None, n);
    TEST_ASSERT_EQUAL(Phase::Connecting, c.step(kTotalTimeoutMs - 1, LinkStatus::Failed, n));
    TEST_ASSERT_EQUAL(Phase::Failed, c.step(kTotalTimeoutMs, LinkStatus::Failed, n));
    TEST_ASSERT_FALSE(c.cachedFailed());
}
void test_connected_wins_over_timeout_at_boundary() {
    Connector c;
    c.start(0, false);
    Attempt n;
    TEST_ASSERT_EQUAL(Phase::Connected, c.step(kTotalTimeoutMs, LinkStatus::Connected, n));
}
void test_failed_is_sticky_and_restart_resets() {
    Connector c;
    c.start(0, true);
    Attempt n;
    c.step(kTotalTimeoutMs, LinkStatus::Connecting, n);
    TEST_ASSERT_EQUAL(Phase::Failed, c.step(kTotalTimeoutMs + 1, LinkStatus::Connected, n));
    c.start(50000, false);
    TEST_ASSERT_FALSE(c.cachedFailed());
    TEST_ASSERT_EQUAL(Phase::Connected, c.step(50100, LinkStatus::Connected, n));
}
void test_fallback_happens_once() {
    Connector c;
    c.start(0, true);
    Attempt n;
    c.step(kCachedAttemptMs, LinkStatus::Failed, n);
    TEST_ASSERT_EQUAL(Attempt::Scan, n);
    c.step(kCachedAttemptMs + 10, LinkStatus::Failed, n);
    TEST_ASSERT_EQUAL(Attempt::None, n);
    TEST_ASSERT_EQUAL(Attempt::Scan, c.current());
}
void test_wrap_total_timeout() {
    Connector c;
    uint32_t t0 = 0xFFFFFFFFu - 1000;
    c.start(t0, false);
    Attempt n;
    TEST_ASSERT_EQUAL(Phase::Connecting, c.step(t0 + kTotalTimeoutMs - 1, LinkStatus::Connecting, n));
    TEST_ASSERT_EQUAL(Phase::Failed, c.step(t0 + kTotalTimeoutMs, LinkStatus::Connecting, n));
}
void test_channel_bounds() {
    CacheRecord r = rec();
    r.channel = 14;
    TEST_ASSERT_TRUE(cacheValid(r, "home", false));
    r.channel = 15;
    TEST_ASSERT_FALSE(cacheValid(r, "home", false));
    r.channel = 0;
    TEST_ASSERT_FALSE(cacheValid(r, "home", false));
}
void test_usable_failure_threshold_and_saturation() {
    CacheRecord r = rec();
    TEST_ASSERT_TRUE(cacheUsable(r, "home", false));
    r = afterCachedFailure(r);
    TEST_ASSERT_TRUE(cacheUsable(r, "home", false));
    r = afterCachedFailure(r);
    TEST_ASSERT_FALSE(cacheUsable(r, "home", false));
    TEST_ASSERT_TRUE(cacheValid(r, "home", false));
    r.failures = 255;
    r = afterCachedFailure(r);
    TEST_ASSERT_EQUAL_UINT8(255, r.failures);
}
void test_lease_requires_ip_gateway_subnet() {
    CacheRecord r = makeRecord("home", kB, 6, false, 1, 2, 3, 0);
    TEST_ASSERT_TRUE(hasLease(r));  // dns optional
    TEST_ASSERT_FALSE(hasLease(makeRecord("home", kB, 6, false, 0, 2, 3, 4)));
    TEST_ASSERT_FALSE(hasLease(makeRecord("home", kB, 6, false, 1, 0, 3, 4)));
    TEST_ASSERT_FALSE(hasLease(makeRecord("home", kB, 6, false, 1, 2, 0, 4)));
}
void test_static_record_drops_lease_values() {
    CacheRecord r = makeRecord("home", kB, 6, true, 9, 9, 9, 9);
    TEST_ASSERT_EQUAL_UINT32(0, r.ip);
    TEST_ASSERT_EQUAL_UINT32(0, r.dns);
    TEST_ASSERT_FALSE(hasLease(r));
}
void test_ssid_hash_case_and_empty() {
    TEST_ASSERT_EQUAL_UINT32(2166136261u, ssidHash(""));
    TEST_ASSERT_NOT_EQUAL(ssidHash("home"), ssidHash("Home"));
    CacheRecord r = rec();
    TEST_ASSERT_FALSE(cacheValid(r, "Home", false));
}
void test_corrupt_magic_invalid() {
    CacheRecord r = rec();
    r.magic ^= 1;
    TEST_ASSERT_FALSE(cacheValid(r, "home", false));
}

void test_same_record_field_by_field() {
    CacheRecord a = rec(), b = rec();
    TEST_ASSERT_TRUE(sameRecord(a, b));
    b.lease_epoch = 1800000000;
    TEST_ASSERT_TRUE(sameRecord(a, b));  // epoch ignored
    b = a; b.failures = 1;
    TEST_ASSERT_FALSE(sameRecord(a, b));
    b = a; b.bssid[5] ^= 1;
    TEST_ASSERT_FALSE(sameRecord(a, b));
    b = a; b.ip = 99;
    TEST_ASSERT_FALSE(sameRecord(a, b));
    b = a; b.channel = 7;
    TEST_ASSERT_FALSE(sameRecord(a, b));
    b = a; b.static_ip = 1;
    TEST_ASSERT_FALSE(sameRecord(a, b));
}
void test_same_record_ignores_padding() {
    CacheRecord a = rec(), b = rec();
    unsigned char *pb = reinterpret_cast<unsigned char *>(&b);
    for (size_t i = 0; i < sizeof(b); ++i) pb[i] ^= 0;  // keep fields
    // scribble over the padding bytes after the 6-byte bssid + channel/failures/static_ip
    pb[offsetof(CacheRecord, static_ip) + 1] = 0xAB;
    TEST_ASSERT_TRUE(sameRecord(a, b));
}
void test_plausible_epoch() {
    TEST_ASSERT_EQUAL_UINT32(0, plausibleEpoch(0));
    TEST_ASSERT_EQUAL_UINT32(0, plausibleEpoch(12345));
    TEST_ASSERT_EQUAL_UINT32(0, plausibleEpoch(-5));
    TEST_ASSERT_EQUAL_UINT32(0, plausibleEpoch(0x1FFFFFFFFll));
    TEST_ASSERT_EQUAL_UINT32(1800000000u, plausibleEpoch(1800000000));
}
void test_lease_age_limit() {
    const uint32_t t = 1800000000;
    CacheRecord r = makeRecord("home", kB, 6, false, 1, 2, 3, 4, t);
    TEST_ASSERT_FALSE(leaseExpired(r, t + kLeaseMaxAgeS));
    TEST_ASSERT_TRUE(leaseExpired(r, t + kLeaseMaxAgeS + 1));
    TEST_ASSERT_FALSE(leaseExpired(r, 0));          // clock unset now
    TEST_ASSERT_TRUE(leaseExpired(r, t - 10));      // clock went backwards
    CacheRecord u = makeRecord("home", kB, 6, false, 1, 2, 3, 4, 0);
    TEST_ASSERT_FALSE(leaseExpired(u, t + 10 * kLeaseMaxAgeS));  // never stamped
}
void test_apply_lease_age_keeps_cache_and_failures() {
    const uint32_t t = 1800000000;
    CacheRecord r = makeRecord("home", kB, 6, false, 1, 2, 3, 4, t);
    r.failures = 1;
    CacheRecord o = applyLeaseAge(r, t + kLeaseMaxAgeS + 1);
    TEST_ASSERT_FALSE(hasLease(o));
    TEST_ASSERT_EQUAL_UINT32(0, o.lease_epoch);
    TEST_ASSERT_EQUAL_UINT8(1, o.failures);
    TEST_ASSERT_TRUE(cacheValid(o, "home", false));
    TEST_ASSERT_TRUE(sameRecord(applyLeaseAge(r, t + 5), r));
}
void test_drop_lease() {
    CacheRecord o = dropLease(rec());
    TEST_ASSERT_FALSE(hasLease(o));
    TEST_ASSERT_TRUE(cacheUsable(o, "home", false));
}
void test_auth_reasons() {
    TEST_ASSERT_TRUE(isAuthReason(202));
    TEST_ASSERT_TRUE(isAuthReason(15));
    TEST_ASSERT_TRUE(isAuthReason(204));
    TEST_ASSERT_TRUE(isAuthReason(14));
    TEST_ASSERT_FALSE(isAuthReason(201));  // NO_AP_FOUND
    TEST_ASSERT_FALSE(isAuthReason(8));
}
void test_auth_failed_not_cache_failure() {
    Connector c;
    c.start(0, true);
    Attempt n;
    TEST_ASSERT_EQUAL(Phase::Failed, c.step(500, LinkStatus::AuthFailed, n));
    TEST_ASSERT_TRUE(c.authFailed());
    TEST_ASSERT_FALSE(c.cachedFailed());
    TEST_ASSERT_EQUAL(Attempt::None, n);
    c.start(1000, true);
    TEST_ASSERT_FALSE(c.authFailed());
}
void test_start_failed_cached_goes_to_scan_then_failed() {
    Connector c;
    c.start(0, true);
    Attempt n;
    TEST_ASSERT_EQUAL(Phase::Connecting, c.startFailed(n));
    TEST_ASSERT_EQUAL(Attempt::Scan, n);
    TEST_ASSERT_TRUE(c.cachedFailed());
    TEST_ASSERT_EQUAL(Phase::Failed, c.startFailed(n));
    TEST_ASSERT_EQUAL(Attempt::None, n);
    Connector d;
    d.start(0, false);
    TEST_ASSERT_EQUAL(Phase::Failed, d.startFailed(n));
    TEST_ASSERT_FALSE(d.cachedFailed());
}


void test_select_no_scan_cases() {
    using namespace netselect;
    TEST_ASSERT_EQUAL(-1, profileWithoutScan(0, 0, -1, false));
    TEST_ASSERT_EQUAL(1, profileWithoutScan(3, 2, -1, false));    // forced: no scan
    TEST_ASSERT_EQUAL(0, profileWithoutScan(1, 0, -1, false));    // single profile
    TEST_ASSERT_EQUAL(0, profileWithoutScan(3, 0, 0, true));      // first one, cache usable
    TEST_ASSERT_EQUAL(-1, profileWithoutScan(3, 0, 0, false));    // cache worn out -> scan
    TEST_ASSERT_EQUAL(1, profileWithoutScan(3, 0, 1, true));      // last was #2: its cache first, scan only on failure
    TEST_ASSERT_EQUAL(-1, profileWithoutScan(3, 0, 1, false));    // #2 but cache unusable -> scan
    TEST_ASSERT_EQUAL(-1, profileWithoutScan(3, 0, 5, true));     // stale index beyond the list -> scan
    TEST_ASSERT_EQUAL(-1, profileWithoutScan(3, 0, -1, false));
    TEST_ASSERT_EQUAL(-1, profileWithoutScan(2, 5, -1, false));   // forced beyond the list = Auto
}
void test_select_direct_failure_falls_back_to_scan() {
    using namespace netselect;
    TEST_ASSERT_TRUE(scanAfterDirectFailure(2, 0, true, false));
    TEST_ASSERT_TRUE(scanAfterDirectFailure(3, 9, true, false));   // forced beyond list = Auto
    TEST_ASSERT_FALSE(scanAfterDirectFailure(2, 0, true, true));   // only once
    TEST_ASSERT_FALSE(scanAfterDirectFailure(2, 0, false, false)); // the attempt was a scan pick
    TEST_ASSERT_FALSE(scanAfterDirectFailure(1, 0, true, false));  // nothing else to pick
    TEST_ASSERT_FALSE(scanAfterDirectFailure(3, 2, true, false));  // forced is exclusive
    // After the failure of profile 0 the scan must pick another one.
    std::vector<std::string> p = {"home", "phone"};
    bool found = false;
    TEST_ASSERT_EQUAL(1, pickAfterScan(p, {"home", "phone"}, 0, &found, 0));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(1, pickAfterScan(p, {"home"}, 0, &found, 0));   // nothing else seen: not 0
    TEST_ASSERT_FALSE(found);
    TEST_ASSERT_EQUAL(-1, pickAfterScan({"home"}, {}, 0, &found, 0));
}
void test_select_after_scan_list_order() {
    using namespace netselect;
    std::vector<std::string> p = {"home", "phone", "office"};
    bool found = false;
    // Both in range: list order wins, not scan order.
    TEST_ASSERT_EQUAL(0, pickAfterScan(p, {"phone", "home"}, 1, &found));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(1, pickAfterScan(p, {"phone", "neighbour"}, 0, &found));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL(2, pickAfterScan(p, {"office"}, -1, &found));
}
void test_select_after_scan_nothing_found() {
    using namespace netselect;
    std::vector<std::string> p = {"home", "phone"};
    bool found = true;
    TEST_ASSERT_EQUAL(1, pickAfterScan(p, {}, 1, &found));   // fall back to the last one
    TEST_ASSERT_FALSE(found);
    TEST_ASSERT_EQUAL(0, pickAfterScan(p, {"x"}, -1, &found));
    TEST_ASSERT_EQUAL(0, pickAfterScan(p, {"x"}, 7, &found)); // bogus last
    TEST_ASSERT_EQUAL(-1, pickAfterScan({}, {"x"}, -1, &found));
}
void test_per_profile_cache_is_tied_to_its_ssid() {
    CacheRecord home = makeRecord("home", kB, 6, false, 1, 2, 3, 4);
    CacheRecord phone = makeRecord("phone", kB, 11, false, 5, 6, 7, 8);
    TEST_ASSERT_TRUE(cacheUsable(home, "home", false));
    TEST_ASSERT_FALSE(cacheUsable(home, "phone", false));   // slot mix-up is rejected
    TEST_ASSERT_TRUE(cacheUsable(phone, "phone", false));
    phone = afterCachedFailure(afterCachedFailure(phone));
    TEST_ASSERT_FALSE(cacheUsable(phone, "phone", false));
    TEST_ASSERT_TRUE(cacheUsable(home, "home", false));      // the other profile is untouched
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_select_no_scan_cases);
    RUN_TEST(test_select_direct_failure_falls_back_to_scan);
    RUN_TEST(test_select_after_scan_list_order);
    RUN_TEST(test_select_after_scan_nothing_found);
    RUN_TEST(test_per_profile_cache_is_tied_to_its_ssid);
    RUN_TEST(test_default_record_invalid);
    RUN_TEST(test_valid_record);
    RUN_TEST(test_wrong_ssid_invalid);
    RUN_TEST(test_static_mode_mismatch_invalid);
    RUN_TEST(test_static_record_has_no_lease);
    RUN_TEST(test_bad_channel_or_bssid);
    RUN_TEST(test_invalidated_after_two_failures);
    RUN_TEST(test_cached_success_fast);
    RUN_TEST(test_cached_timeout_falls_back_to_scan);
    RUN_TEST(test_cached_driver_failure_falls_back_early);
    RUN_TEST(test_overall_timeout);
    RUN_TEST(test_scan_start_never_uses_cached);
    RUN_TEST(test_millis_wraparound);
    RUN_TEST(test_timeout_with_cached_active_counts_failure);
    RUN_TEST(test_cached_fallback_boundary);
    RUN_TEST(test_scan_driver_failure_waits_for_total_timeout);
    RUN_TEST(test_connected_wins_over_timeout_at_boundary);
    RUN_TEST(test_failed_is_sticky_and_restart_resets);
    RUN_TEST(test_fallback_happens_once);
    RUN_TEST(test_wrap_total_timeout);
    RUN_TEST(test_channel_bounds);
    RUN_TEST(test_usable_failure_threshold_and_saturation);
    RUN_TEST(test_lease_requires_ip_gateway_subnet);
    RUN_TEST(test_static_record_drops_lease_values);
    RUN_TEST(test_ssid_hash_case_and_empty);
    RUN_TEST(test_corrupt_magic_invalid);
    RUN_TEST(test_same_record_field_by_field);
    RUN_TEST(test_same_record_ignores_padding);
    RUN_TEST(test_plausible_epoch);
    RUN_TEST(test_lease_age_limit);
    RUN_TEST(test_apply_lease_age_keeps_cache_and_failures);
    RUN_TEST(test_drop_lease);
    RUN_TEST(test_auth_reasons);
    RUN_TEST(test_auth_failed_not_cache_failure);
    RUN_TEST(test_start_failed_cached_goes_to_scan_then_failed);
    return UNITY_END();
}
