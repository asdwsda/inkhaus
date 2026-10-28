#include "net_policy.h"

namespace netpolicy {

uint32_t ssidHash(const std::string &ssid) {
    uint32_t h = 2166136261u;
    for (unsigned char c : ssid) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

static bool bssidNonZero(const uint8_t b[6]) {
    for (int i = 0; i < 6; ++i)
        if (b[i]) return true;
    return false;
}

bool cacheValid(const CacheRecord &r, const std::string &ssid, bool staticIpConfigured) {
    if (r.magic != kCacheMagic) return false;
    if (r.ssid_hash != ssidHash(ssid)) return false;
    if (!bssidNonZero(r.bssid)) return false;
    if (r.channel < 1 || r.channel > 14) return false;
    if ((r.static_ip != 0) != staticIpConfigured) return false;
    return true;
}

bool cacheUsable(const CacheRecord &r, const std::string &ssid, bool staticIpConfigured) {
    return cacheValid(r, ssid, staticIpConfigured) && r.failures < kMaxCacheFailures;
}

bool sameRecord(const CacheRecord &a, const CacheRecord &b) {
    for (int i = 0; i < 6; ++i)
        if (a.bssid[i] != b.bssid[i]) return false;
    return a.magic == b.magic && a.ssid_hash == b.ssid_hash && a.channel == b.channel &&
           a.failures == b.failures && a.static_ip == b.static_ip && a.ip == b.ip &&
           a.gateway == b.gateway && a.subnet == b.subnet && a.dns == b.dns;
}

uint32_t plausibleEpoch(int64_t raw) {
    return (raw >= (int64_t)kMinPlausibleEpoch && raw <= 0xFFFFFFFFll) ? (uint32_t)raw : 0;
}

bool leaseExpired(const CacheRecord &r, uint32_t now) {
    if (!now || !r.lease_epoch || !hasLease(r)) return false;
    if (now < r.lease_epoch) return true;
    return now - r.lease_epoch > kLeaseMaxAgeS;
}

CacheRecord dropLease(CacheRecord r) {
    r.ip = r.gateway = r.subnet = r.dns = 0;
    r.lease_epoch = 0;
    return r;
}

CacheRecord applyLeaseAge(CacheRecord r, uint32_t now) {
    return leaseExpired(r, now) ? dropLease(r) : r;
}

bool isAuthReason(uint8_t reason) {
    return reason == 14 || reason == 15 || reason == 202 || reason == 204;
}

bool hasLease(const CacheRecord &r) {
    return !r.static_ip && r.ip && r.gateway && r.subnet;
}

CacheRecord makeRecord(const std::string &ssid, const uint8_t bssid[6], uint8_t channel,
                       bool staticIpConfigured, uint32_t ip, uint32_t gateway, uint32_t subnet,
                       uint32_t dns, uint32_t nowEpoch) {
    CacheRecord r;
    r.magic = kCacheMagic;
    r.ssid_hash = ssidHash(ssid);
    for (int i = 0; i < 6; ++i) r.bssid[i] = bssid[i];
    r.channel = channel;
    r.static_ip = staticIpConfigured ? 1 : 0;
    if (!staticIpConfigured) {
        r.ip = ip;
        r.gateway = gateway;
        r.subnet = subnet;
        r.dns = dns;
        r.lease_epoch = nowEpoch;
    }
    return r;
}

CacheRecord afterCachedFailure(CacheRecord r) {
    if (r.failures < 255) r.failures++;
    return r;
}

Attempt Connector::start(uint32_t nowMs, bool useCache) {
    t0_ = attemptT0_ = nowMs;
    cachedFailed_ = false;
    authFailed_ = false;
    phase_ = Phase::Connecting;
    current_ = useCache ? Attempt::Cached : Attempt::Scan;
    return current_;
}

Phase Connector::step(uint32_t nowMs, LinkStatus link, Attempt &startNew) {
    startNew = Attempt::None;
    if (phase_ != Phase::Connecting) return phase_;
    if (link == LinkStatus::Connected) return phase_ = Phase::Connected;
    if (link == LinkStatus::AuthFailed) {
        authFailed_ = true;
        current_ = Attempt::None;
        return phase_ = Phase::Failed;
    }
    if (nowMs - t0_ >= kTotalTimeoutMs) {
        if (current_ == Attempt::Cached) cachedFailed_ = true;
        current_ = Attempt::None;
        return phase_ = Phase::Failed;
    }
    bool failed = link == LinkStatus::Failed;
    bool cachedExpired = current_ == Attempt::Cached && nowMs - attemptT0_ >= kCachedAttemptMs;
    if (current_ == Attempt::Cached && (failed || cachedExpired)) {
        cachedFailed_ = true;
        current_ = startNew = Attempt::Scan;
        attemptT0_ = nowMs;
    }
    // A failed scan attempt is left to run until the overall timeout: the driver may retry.
    return phase_;
}

Phase Connector::startFailed(Attempt &startNew) {
    startNew = Attempt::None;
    if (phase_ != Phase::Connecting) return phase_;
    if (current_ == Attempt::Cached) {
        cachedFailed_ = true;
        current_ = startNew = Attempt::Scan;
        return phase_;
    }
    current_ = Attempt::None;
    return phase_ = Phase::Failed;
}

}  // namespace netpolicy
