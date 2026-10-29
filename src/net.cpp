#include "net.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstring>
#include <ctime>

#include <vector>

#include "net_policy.h"
#include "wifi_select.h"
#include "wakelog.h"

using namespace netpolicy;

namespace {

// One fast-reconnect cache per profile (task 047). Validated by magic + SSID hash, so garbage is harmless.
RTC_NOINIT_ATTR CacheRecord g_rtc[netselect::kMaxProfiles];
struct LastProfile {
    uint32_t magic;
    int32_t index;
};
constexpr uint32_t kLastMagic = 0x4C505231;
RTC_NOINIT_ATTR LastProfile g_last;  // profile that connected last (survives deep sleep)

std::vector<config::WifiConfig> g_profiles;
bool g_configured = false;
int g_forced = 0;   // 0 = Auto, else 1-based
int g_haLink = 0;   // settings::HaLink as int
int g_active = 0;   // index into g_profiles of the profile in use
bool g_scanning = false;
uint32_t g_scanT0 = 0;
constexpr uint32_t kScanTimeoutMs = 6000;
// Read by the network task (core 0): plain words, written when the link comes up / goes down.
volatile int g_linkProfile = -1;
volatile uint32_t g_linkHash = 0;
volatile int g_linkHaUrl = 0;

config::WifiConfig g_cfg;
Connector g_conn;
net::State g_state = net::State::Idle;
CacheRecord g_cache;
bool g_cacheLoaded = false;
CacheRecord g_persisted;  // what NVS holds (default = nothing valid), to write only on change
volatile bool g_authLatch = false;  // wrong-password disconnect reason seen in this attempt
bool g_eventsHooked = false;

uint32_t nowEpoch() { return plausibleEpoch((int64_t)time(nullptr)); }

bool g_usingCache = false;
uint32_t g_connectMs = 0;
uint32_t g_startMs = 0;

void logSsid(const char *phase) {  // length + first 2 chars only
    // SSID only as length + first 2 chars; never the password.
    char pre[3] = {0, 0, 0};
    for (int i = 0; i < 2 && i < (int)g_cfg.ssid.size(); ++i) pre[i] = g_cfg.ssid[i];
    wakelog::logf("NET: phase=%s profile=%d ssid_len=%u ssid_pre=%s\n", phase, g_active,
                  (unsigned)g_cfg.ssid.size(), pre);
}

void slotKey(int idx, char *out) { snprintf(out, 8, "rec%d", idx); }

bool nvsLoad(int idx, CacheRecord &r) {
    Preferences p;
    if (!p.begin("netcache", true)) return false;
    char k[8];
    slotKey(idx, k);
    bool ok = p.getBytesLength(k) == sizeof(r) && p.getBytes(k, &r, sizeof(r)) == sizeof(r);
    p.end();
    return ok;
}

// Writes only if r differs from what NVS already holds (flash wear).
void nvsSave(const CacheRecord &r) {
    if (sameRecord(r, g_persisted)) return;
    Preferences p;
    if (!p.begin("netcache", false)) return;
    char k[8];
    slotKey(g_active, k);
    p.putBytes(k, &r, sizeof(r));
    p.end();
    g_persisted = r;
}

void nvsClear() {
    Preferences p;
    if (!p.begin("netcache", false)) return;
    char k[8];
    slotKey(g_active, k);
    p.remove(k);
    p.end();
    g_persisted = CacheRecord{};
}

bool lanIsStatic() { return g_cfg.hasStaticIp; }

// Cache of profile idx (RTC first, then NVS), lease aged. `persisted` gets what NVS holds.
CacheRecord loadSlot(int idx, CacheRecord *persisted = nullptr) {
    const config::WifiConfig &w = g_profiles[idx];
    CacheRecord cache, n;
    bool haveNvs = nvsLoad(idx, n);
    if (persisted) *persisted = haveNvs ? n : CacheRecord{};
    // RTC survives deep sleep / ESP.restart; NVS survives power-off.
    if (cacheValid(g_rtc[idx], w.ssid, w.hasStaticIp)) {
        cache = g_rtc[idx];
    } else if (haveNvs && cacheValid(n, w.ssid, w.hasStaticIp)) {
        cache = n;
    }
    CacheRecord aged = applyLeaseAge(cache, nowEpoch());
    if (!sameRecord(aged, cache)) wakelog::logf("NET: lease=expired" "\n");
    return aged;
}

int lastProfile() {
    if (g_last.magic != kLastMagic) return -1;
    return (g_last.index >= 0 && g_last.index < (int)g_profiles.size()) ? g_last.index : -1;
}

void activate(int idx) {
    g_active = idx;
    g_cfg = g_profiles[idx];
    g_persisted = CacheRecord{};
    g_cache = loadSlot(idx, &g_persisted);
    g_cacheLoaded = true;
}

void applyIpConfig(bool useLease) {
    if (g_cfg.hasStaticIp) {
        const auto &s = g_cfg.staticIp;
        IPAddress ip, gw, sn, dns;
        if (ip.fromString(s.ip.c_str()) && gw.fromString(s.gateway.c_str()) &&
            sn.fromString(s.subnet.c_str())) {
            if (!s.dns.empty()) dns.fromString(s.dns.c_str());
            WiFi.config(ip, gw, sn, dns);
            return;
        }
    }
    if (useLease && hasLease(g_cache)) {
        WiFi.config(IPAddress(g_cache.ip), IPAddress(g_cache.gateway), IPAddress(g_cache.subnet),
                    IPAddress(g_cache.dns));
        return;
    }
    WiFi.config(IPAddress(0u), IPAddress(0u), IPAddress(0u));  // back to DHCP
}

// Returns false if WiFi.begin refused to start (it returns WL_CONNECT_FAILED then).
bool startAttempt(Attempt a) {
    g_authLatch = false;
    WiFi.disconnect(false, false);
    wl_status_t st;
    if (a == Attempt::Cached) {
        applyIpConfig(true);
        st = WiFi.begin(g_cfg.ssid.c_str(), g_cfg.password.c_str(), g_cache.channel, g_cache.bssid);
        wakelog::logf("NET: phase=attempt_cached ch=%u lease=%d\n", g_cache.channel,
                      hasLease(g_cache) ? 1 : 0);
    } else {
        applyIpConfig(false);
        st = WiFi.begin(g_cfg.ssid.c_str(), g_cfg.password.c_str());
        wakelog::logf("NET: phase=attempt_scan" "\n");
    }
    if (st == WL_CONNECT_FAILED) {
        wakelog::logf("NET: begin_failed attempt=%s\n", a == Attempt::Cached ? "cached" : "scan");
        return false;
    }
    return true;
}

// Starts `a`; if begin() itself fails, lets the state machine move on (cached -> scan -> Failed).
Phase launch(Attempt a) {
    Phase ph = Phase::Connecting;
    while (a != Attempt::None && !startAttempt(a)) {
        Attempt next = Attempt::None;
        ph = g_conn.startFailed(next);
        a = next;
    }
    return ph;
}

void remember() {
    uint8_t bssid[6] = {0};
    const uint8_t *b = WiFi.BSSID();
    if (b) memcpy(bssid, b, 6);
    CacheRecord r = makeRecord(g_cfg.ssid, bssid, (uint8_t)WiFi.channel(), lanIsStatic(),
                               (uint32_t)WiFi.localIP(), (uint32_t)WiFi.gatewayIP(),
                               (uint32_t)WiFi.subnetMask(), (uint32_t)WiFi.dnsIP(), nowEpoch());
    r.lease_epoch = hasLease(r) ? nowEpoch() : 0;
    g_cache = r;
    g_rtc[g_active] = r;
    nvsSave(r);  // only writes when something differs (flash wear)
    g_last.magic = kLastMagic;
    g_last.index = g_active;
}

void publishLink(bool up) {
    g_linkProfile = up ? g_active : -1;
    g_linkHaUrl = up ? (int)g_cfg.haUrl : 0;
    g_linkHash = up ? ssidHash(g_cfg.ssid) : 0;
}

// Connects with the active profile (cache or scan attempt).
void connectActive() {
    logSsid("begin");
    g_connectMs = 0;
    bool use = cacheUsable(g_cache, g_cfg.ssid, lanIsStatic());
    g_usingCache = use;
    wakelog::logf("NET: cache=%s failures=%u\n", use ? "hit" : (g_cache.magic ? "invalid" : "miss"),
                  (unsigned)g_cache.failures);
    g_startMs = millis();
    g_state = net::State::Connecting;
    if (launch(g_conn.start(g_startMs, use)) == Phase::Failed) {
        wakelog::logf("NET: phase=failed reason=begin" "\n");
        g_state = net::State::Failed;
    }
}

// Auto, nothing to skip the scan: start an asynchronous scan for the known SSIDs.
void startScan() {
    g_scanning = true;
    g_scanT0 = millis();
    g_state = net::State::Connecting;
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 300);  // UNVERIFIED: async scan with 300 ms/channel dwell finds the APs in <= ~4 s
    wakelog::logf("NET: phase=scan profiles=%u\n", (unsigned)g_profiles.size());
}

void finishScan() {
    std::vector<std::string> seen;
    int n = WiFi.scanComplete();
    for (int i = 0; i < n; ++i) seen.push_back(WiFi.SSID(i).c_str());
    WiFi.scanDelete();
    g_scanning = false;
    std::vector<std::string> ssids;
    for (const auto &w : g_profiles) ssids.push_back(w.ssid);
    bool found = false;
    int pick = netselect::pickAfterScan(ssids, seen, lastProfile(), &found);
    wakelog::logf("NET: scan_done aps=%d found=%d pick=%d ms=%u\n", n, found ? 1 : 0, pick,
                  (unsigned)(millis() - g_scanT0));
    activate(pick);
    connectActive();
}

LinkStatus linkStatus() {
    wl_status_t st = WiFi.status();
    // Arduino 2.0.17: after an auth failure on the first connect the driver retries by itself
    // and status() may flip back to IDLE/DISCONNECTED, so the reason is latched by the event.
    if (st != WL_CONNECTED && g_authLatch) return LinkStatus::AuthFailed;
    switch (st) {
        case WL_CONNECTED: return LinkStatus::Connected;
        case WL_CONNECT_FAILED:
        case WL_NO_SSID_AVAIL: return LinkStatus::Failed;
        default: return LinkStatus::Connecting;
    }
}

}  // namespace

namespace net {

void configure(const std::vector<config::WifiConfig> &profiles, int forcedProfile, int haLink) {
    g_profiles = profiles;
    if (g_profiles.size() > (size_t)netselect::kMaxProfiles) g_profiles.resize(netselect::kMaxProfiles);
    g_forced = forcedProfile;
    g_haLink = haLink;
    g_configured = true;
}

void begin(const config::WifiConfig &wifi) {
    if (!g_configured) {  // single-network callers (tls_check)
        g_profiles.assign(1, wifi);
        g_forced = 0;
    }
    g_connectMs = 0;
    g_scanning = false;
    publishLink(false);
    if (g_profiles.empty() || g_profiles[0].ssid.empty()) {
        wakelog::logf("NET: phase=failed reason=no_ssid" "\n");
        g_state = State::Failed;
        return;
    }
    if (!g_eventsHooked) {
        g_eventsHooked = true;
        WiFi.onEvent(
            [](arduino_event_id_t, arduino_event_info_t info) {
                if (isAuthReason(info.wifi_sta_disconnected.reason)) g_authLatch = true;
            },
            ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    }
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // full-rate radio while connecting; powerDown() releases it
    const int last = lastProfile();
    bool lastUsable = false;
    if (last >= 0 && g_forced == 0) {
        CacheRecord c = loadSlot(last);
        lastUsable = cacheUsable(c, g_profiles[last].ssid, g_profiles[last].hasStaticIp);
    }
    const int pick = netselect::profileWithoutScan((int)g_profiles.size(), g_forced, last, lastUsable);
    if (pick >= 0) {
        activate(pick);
        connectActive();
    } else {
        startScan();
    }
}

State poll() {
    if (g_state == State::Connected) {
        if (WiFi.status() != WL_CONNECTED) {
            wakelog::logf("NET: phase=link_lost" "\n");
            g_state = State::Failed;
        }
        return g_state;
    }
    if (g_state != State::Connecting) return g_state;
    if (g_scanning) {
        const bool running = WiFi.scanComplete() == WIFI_SCAN_RUNNING;
        if (running && millis() - g_scanT0 < kScanTimeoutMs) return g_state;
        finishScan();
        if (g_state != State::Connecting) return g_state;
    }

    uint32_t now = millis();
    Attempt next = Attempt::None;
    Phase ph = g_conn.step(now, linkStatus(), next);
    if (next != Attempt::None) {
        wakelog::logf("NET: phase=cache_failed_fallback_scan" "\n");
        if (launch(next) == Phase::Failed) ph = Phase::Failed;
    }
    if (ph == Phase::Connected) {
        g_connectMs = now - g_startMs;
        g_state = State::Connected;
        if (g_usingCache && g_conn.cachedFailed()) wakelog::logf("NET: cache=stale_rescanned" "\n");
        remember();
        publishLink(true);
        wakelog::logf("NET: phase=connected connect_ms=%u rssi=%d ch=%d\n", (unsigned)g_connectMs,
                      WiFi.RSSI(), WiFi.channel());
    } else if (ph == Phase::Failed) {
        g_state = State::Failed;
        if (g_conn.authFailed()) {
            wakelog::logf("NET: auth_failed" "\n");  // wrong password: says nothing about the cache
        } else if (g_conn.cachedFailed() && g_usingCache) {
            CacheRecord r = afterCachedFailure(g_cache);
            g_cache = r;
            g_rtc[g_active] = r;
            if (!cacheUsable(r, g_cfg.ssid, lanIsStatic())) {
                wakelog::logf("NET: cache=invalid" "\n");
                g_rtc[g_active] = CacheRecord{};
                nvsClear();
            } else {
                nvsSave(r);  // keep the failure count across a power cycle
            }
        }
        wakelog::logf("NET: phase=failed connect_ms=%u\n", (unsigned)(now - g_startMs));
        WiFi.disconnect(false, false);
    }
    return g_state;
}

void invalidateLease() {
    if (!g_cacheLoaded || !hasLease(g_cache)) return;
    CacheRecord r = dropLease(g_cache);
    g_cache = r;
    if (cacheValid(g_rtc[g_active], g_cfg.ssid, lanIsStatic())) g_rtc[g_active] = r;
    nvsSave(r);
    wakelog::logf("NET: lease=invalidated" "\n");
}

void stop() {
    publishLink(false);
    g_scanning = false;
    WiFi.disconnect(false, false);
    g_state = State::Idle;
}

void powerDown() {
    publishLink(false);
    g_scanning = false;
    WiFi.disconnect(true, false);
    WiFi.setSleep(false);  // release modem sleep before the radio is switched off
    WiFi.mode(WIFI_OFF);
    g_state = State::Idle;
    wakelog::logf("NET: phase=power_down" "\n");
}

int rssi() { return g_state == State::Connected ? WiFi.RSSI() : 0; }
int channel() { return g_state == State::Connected ? WiFi.channel() : 0; }
uint32_t connectMs() { return g_connectMs; }

LinkInfo linkInfo() {
    LinkInfo i;
    i.profile = g_linkProfile;
    i.ssidHash = g_linkHash;
    i.haUrl = g_linkHaUrl;
    i.haLink = g_haLink;
    return i;
}

std::string activeProfileName() {
    return (g_linkProfile >= 0 && g_linkProfile < (int)g_profiles.size()) ? g_profiles[g_linkProfile].name : "";
}

}  // namespace net
