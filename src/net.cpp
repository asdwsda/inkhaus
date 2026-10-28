#include "net.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstring>
#include <ctime>

#include "net_policy.h"

using namespace netpolicy;

namespace {

RTC_NOINIT_ATTR CacheRecord g_rtc;  // validated by magic + hash, so garbage is harmless

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

void logSsid(const char *phase) {
    // SSID only as length + first 2 chars; never the password.
    char pre[3] = {0, 0, 0};
    for (int i = 0; i < 2 && i < (int)g_cfg.ssid.size(); ++i) pre[i] = g_cfg.ssid[i];
    Serial.printf("NET: phase=%s ssid_len=%u ssid_pre=%s\n", phase, (unsigned)g_cfg.ssid.size(), pre);
}

bool nvsLoad(CacheRecord &r) {
    Preferences p;
    if (!p.begin("netcache", true)) return false;
    bool ok = p.getBytesLength("rec") == sizeof(r) && p.getBytes("rec", &r, sizeof(r)) == sizeof(r);
    p.end();
    return ok;
}

// Writes only if r differs from what NVS already holds (flash wear).
void nvsSave(const CacheRecord &r) {
    if (sameRecord(r, g_persisted)) return;
    Preferences p;
    if (!p.begin("netcache", false)) return;
    p.putBytes("rec", &r, sizeof(r));
    p.end();
    g_persisted = r;
}

void nvsClear() {
    Preferences p;
    if (!p.begin("netcache", false)) return;
    p.remove("rec");
    p.end();
    g_persisted = CacheRecord{};
}

bool lanIsStatic() { return g_cfg.hasStaticIp; }

void loadCache() {
    g_cache = CacheRecord{};
    g_persisted = CacheRecord{};
    CacheRecord n;
    bool haveNvs = nvsLoad(n);
    if (haveNvs) g_persisted = n;
    // RTC survives deep sleep / ESP.restart; NVS survives power-off.
    if (cacheValid(g_rtc, g_cfg.ssid, lanIsStatic())) {
        g_cache = g_rtc;
    } else if (haveNvs && cacheValid(n, g_cfg.ssid, lanIsStatic())) {
        g_cache = n;
    }
    CacheRecord aged = applyLeaseAge(g_cache, nowEpoch());
    if (!sameRecord(aged, g_cache)) Serial.println("NET: lease=expired");
    g_cache = aged;
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
        Serial.printf("NET: phase=attempt_cached ch=%u lease=%d\n", g_cache.channel,
                      hasLease(g_cache) ? 1 : 0);
    } else {
        applyIpConfig(false);
        st = WiFi.begin(g_cfg.ssid.c_str(), g_cfg.password.c_str());
        Serial.println("NET: phase=attempt_scan");
    }
    if (st == WL_CONNECT_FAILED) {
        Serial.printf("NET: begin_failed attempt=%s\n", a == Attempt::Cached ? "cached" : "scan");
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
    g_rtc = r;
    nvsSave(r);  // only writes when something differs (flash wear)
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

void begin(const config::WifiConfig &wifi) {
    g_cfg = wifi;
    g_connectMs = 0;
    if (g_cfg.ssid.empty()) {
        Serial.println("NET: phase=failed reason=no_ssid");
        g_state = State::Failed;
        return;
    }
    logSsid("begin");
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
    loadCache();
    bool use = cacheUsable(g_cache, g_cfg.ssid, lanIsStatic());
    g_usingCache = use;
    Serial.printf("NET: cache=%s failures=%u\n", use ? "hit" : (g_cache.magic ? "invalid" : "miss"),
                  (unsigned)g_cache.failures);
    g_startMs = millis();
    g_state = State::Connecting;
    if (launch(g_conn.start(g_startMs, use)) == Phase::Failed) {
        Serial.println("NET: phase=failed reason=begin");
        g_state = State::Failed;
    }
}

State poll() {
    if (g_state == State::Connected) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("NET: phase=link_lost");
            g_state = State::Failed;
        }
        return g_state;
    }
    if (g_state != State::Connecting) return g_state;

    uint32_t now = millis();
    Attempt next = Attempt::None;
    Phase ph = g_conn.step(now, linkStatus(), next);
    if (next != Attempt::None) {
        Serial.println("NET: phase=cache_failed_fallback_scan");
        if (launch(next) == Phase::Failed) ph = Phase::Failed;
    }
    if (ph == Phase::Connected) {
        g_connectMs = now - g_startMs;
        g_state = State::Connected;
        if (g_usingCache && g_conn.cachedFailed()) Serial.println("NET: cache=stale_rescanned");
        remember();
        Serial.printf("NET: phase=connected connect_ms=%u rssi=%d ch=%d\n", (unsigned)g_connectMs,
                      WiFi.RSSI(), WiFi.channel());
    } else if (ph == Phase::Failed) {
        g_state = State::Failed;
        if (g_conn.authFailed()) {
            Serial.println("NET: auth_failed");  // wrong password: says nothing about the cache
        } else if (g_conn.cachedFailed() && g_usingCache) {
            CacheRecord r = afterCachedFailure(g_cache);
            g_cache = r;
            g_rtc = r;
            if (!cacheUsable(r, g_cfg.ssid, lanIsStatic())) {
                Serial.println("NET: cache=invalid");
                g_rtc = CacheRecord{};
                nvsClear();
            } else {
                nvsSave(r);  // keep the failure count across a power cycle
            }
        }
        Serial.printf("NET: phase=failed connect_ms=%u\n", (unsigned)(now - g_startMs));
        WiFi.disconnect(false, false);
    }
    return g_state;
}

void invalidateLease() {
    if (!g_cacheLoaded || !hasLease(g_cache)) return;
    CacheRecord r = dropLease(g_cache);
    g_cache = r;
    if (cacheValid(g_rtc, g_cfg.ssid, lanIsStatic())) g_rtc = r;
    nvsSave(r);
    Serial.println("NET: lease=invalidated");
}

void stop() {
    WiFi.disconnect(false, false);
    g_state = State::Idle;
}

void powerDown() {
    WiFi.disconnect(true, false);
    WiFi.setSleep(false);  // release modem sleep before the radio is switched off
    WiFi.mode(WIFI_OFF);
    g_state = State::Idle;
    Serial.println("NET: phase=power_down");
}

int rssi() { return g_state == State::Connected ? WiFi.RSSI() : 0; }
int channel() { return g_state == State::Connected ? WiFi.channel() : 0; }
uint32_t connectMs() { return g_connectMs; }

}  // namespace net
