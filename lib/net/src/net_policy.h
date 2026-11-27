#pragma once
// Wi-Fi reconnect policy: cache record validity and the connect state machine.
// Pure C++, no Arduino includes, no hardware access (tested natively).
#include <cstdint>
#include <string>

namespace netpolicy {

constexpr uint32_t kTotalTimeoutMs = 8000;     // overall connect budget
constexpr uint32_t kSessionBudgetMs = 15000;   // total connect time per net::begin (scan + attempts, all profiles)
constexpr uint32_t kCachedAttemptMs = 3000;    // cached BSSID/channel attempt before scan fallback
constexpr uint8_t kMaxCacheFailures = 2;       // cache invalidated at this many failures

// What a successful connect left behind. ip/gateway/subnet/dns are host-order-agnostic
// opaque 32-bit values; 0 = unknown. Stored in RTC memory and NVS by src/net.cpp.
struct CacheRecord {
    uint32_t magic = 0;
    uint32_t ssid_hash = 0;   // ties the record to the configured SSID
    uint8_t bssid[6] = {0};
    uint8_t channel = 0;
    uint8_t failures = 0;     // consecutive failed cached attempts
    uint8_t static_ip = 0;    // 1 if config used a static IP when recorded
    uint32_t ip = 0, gateway = 0, subnet = 0, dns = 0;  // DHCP lease (only if !static_ip)
    uint32_t lease_epoch = 0; // wall-clock (Unix s) when the lease was last used; 0 = clock unset
    uint8_t rescan = 0;       // task 137: last cached connect was weak; scan on the next wake
    uint32_t scan_epoch = 0;  // wall-clock (Unix s) of the last wake that scanned; 0 = unknown
};

constexpr uint32_t kCacheMagic = 0x4E455433;  // "NET3" (layout with rescan/scan_epoch)
constexpr uint32_t kLeaseMaxAgeS = 24 * 3600;   // cached lease older than this is dropped
constexpr uint32_t kMinPlausibleEpoch = 1700000000;  // below this the RTC counts as unset

// Field-by-field equality (never memcmp: the struct has padding). lease_epoch is ignored:
// it is stamped on every connect and not worth a flash write on its own.
bool sameRecord(const CacheRecord &a, const CacheRecord &b);
// 0 if the clock is unset/implausible, else the epoch itself.
uint32_t plausibleEpoch(int64_t rawEpoch);
// True if the lease is older than kLeaseMaxAgeS. Unknown clock (now or stamp == 0): never.
// A clock that went backwards past the stamp counts as expired (cannot be trusted).
bool leaseExpired(const CacheRecord &rec, uint32_t nowEpoch);
// Record without its DHCP lease (BSSID/channel/failures kept).
CacheRecord dropLease(CacheRecord rec);
// dropLease() if leaseExpired(), else unchanged.
CacheRecord applyLeaseAge(CacheRecord rec, uint32_t nowEpoch);
// True if the disconnect reason code means "wrong password" (ESP32 wifi_err_reason_t:
// MIC_FAILURE 14, 4WAY_HANDSHAKE_TIMEOUT 15, AUTH_FAIL 202, HANDSHAKE_TIMEOUT 204).
bool isAuthReason(uint8_t reason);

uint32_t ssidHash(const std::string &ssid);  // FNV-1a

// Task 137: prefer a clearly stronger AP of the same SSID over the cached one.
constexpr int kStrongerApDb = 8;        // candidate must beat the cached BSSID by at least this
constexpr int kWeakRssiDbm = -75;       // cached connect below this marks a re-evaluation scan
constexpr uint32_t kRescanMinGapS = 24 * 3600;
struct ApCandidate {
    uint8_t bssid[6] = {0};
    uint8_t channel = 0;
    int rssi = -127;
};
// Index of the candidate to connect to instead of the cached BSSID, or -1 to keep the cache.
// Only a candidate >= kStrongerApDb stronger than the cached BSSID (seen in this scan) wins; the
// strongest such one is chosen. A cached BSSID missing from the scan: -1 (keep the cache).
int pickStrongerAp(const CacheRecord &cache, const ApCandidate *cands, int n);
// Record switched to the candidate's BSSID/channel; failures cleared, rescan state kept.
CacheRecord withAp(CacheRecord rec, const ApCandidate &ap);
// After a connect on the cached fast path: sets rescan if rssi < kWeakRssiDbm and the last scan is
// more than 24 h ago. Unknown clock (nowEpoch 0): unchanged (the limit cannot be kept).
CacheRecord afterCachedConnect(CacheRecord rec, int rssi, uint32_t nowEpoch);
// After a wake that scanned: flag cleared, scan time stamped.
CacheRecord afterScan(CacheRecord rec, uint32_t nowEpoch);

// Structurally valid record for this config (does not look at failures).
bool cacheValid(const CacheRecord &rec, const std::string &ssid, bool staticIpConfigured);
// Valid and not worn out by failures: may be used for the next attempt.
bool cacheUsable(const CacheRecord &rec, const std::string &ssid, bool staticIpConfigured);
// True if the record carries a usable DHCP lease to skip DHCP with.
bool hasLease(const CacheRecord &rec);

CacheRecord makeRecord(const std::string &ssid, const uint8_t bssid[6], uint8_t channel,
                       bool staticIpConfigured, uint32_t ip, uint32_t gateway, uint32_t subnet,
                       uint32_t dns, uint32_t nowEpoch = 0);
// Record after a failed cached attempt: failures+1.
CacheRecord afterCachedFailure(CacheRecord rec);

enum class Attempt { None, Cached, Scan };
// Failed: driver gave up early. AuthFailed: wrong password (never a cache failure).
enum class LinkStatus { Idle, Connecting, Connected, Failed, AuthFailed };
enum class Phase { Connecting, Connected, Failed };

// Total connect time per begin() (task 080). Each stage (scan, attempt, fallback scan, next profile) has
// its own limit, but together they may not exceed kSessionBudgetMs. Monotonic ms, wrap-safe.
class ConnectBudget {
public:
    void start(uint32_t nowMs) { t0_ = nowMs; }
    uint32_t elapsedMs(uint32_t nowMs) const { return nowMs - t0_; }
    uint32_t remainingMs(uint32_t nowMs) const {
        const uint32_t e = nowMs - t0_;
        return e >= kSessionBudgetMs ? 0 : kSessionBudgetMs - e;
    }
    bool expired(uint32_t nowMs) const { return remainingMs(nowMs) == 0; }
    // Limit for the next attempt: its own 8 s, cut to what is left of the budget.
    uint32_t attemptLimitMs(uint32_t nowMs) const {
        const uint32_t r = remainingMs(nowMs);
        return r < kTotalTimeoutMs ? r : kTotalTimeoutMs;
    }
private:
    uint32_t t0_ = 0;
};

// Connect state machine. Time is passed in (ms, monotonic); no waiting inside.
class Connector {
public:
    // Starts the first attempt: Cached if useCache else Scan. limitMs caps the overall timeout
    // (default and maximum kTotalTimeoutMs).
    Attempt start(uint32_t nowMs, bool useCache, uint32_t limitMs = kTotalTimeoutMs);
    // Call periodically. `startNew` is set to the attempt the caller must start now
    // (Attempt::None otherwise). Returns the overall phase.
    Phase step(uint32_t nowMs, LinkStatus link, Attempt &startNew);

    // The caller could not even start the attempt (WiFi.begin returned an error). Cached ->
    // counts as a cache failure and moves to Scan (startNew=Scan, Connecting); Scan -> Failed.
    Phase startFailed(Attempt &startNew);

    Attempt current() const { return current_; }
    bool cachedFailed() const { return cachedFailed_; }  // cached attempt failed this connect
    bool authFailed() const { return authFailed_; }      // ended Failed because of the password
    bool authRetried() const { return authRetried_; }    // first AuthFailed was answered with a retry
    uint32_t elapsedMs(uint32_t nowMs) const { return nowMs - t0_; }

private:
    Attempt current_ = Attempt::None;
    Phase phase_ = Phase::Failed;
    bool cachedFailed_ = false;
    bool authFailed_ = false;
    bool authRetried_ = false;
    uint32_t t0_ = 0, attemptT0_ = 0, limit_ = kTotalTimeoutMs;
};

}  // namespace netpolicy
