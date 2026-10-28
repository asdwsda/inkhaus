# 021: Wi-Fi connection module (fast reconnect)

Status: in-progress
Depends on: 003
UI spec: none
Needs device: yes for the final check (needs the user's config on SD, task 020)

## Scope
`src/net.{h,cpp}` (new) plus pure helpers in `lib/net` (new) with tests.
- Connect to the Wi-Fi from the config model (`config::Connection`): SSID, password, optional static IP.
- Fast reconnect: cache BSSID + channel (+ the DHCP lease's IP/gateway/DNS if no static IP) in RTC
  memory and NVS after a successful connect; the next connect tries the cached BSSID/channel
  first (`WiFi.begin(ssid, pass, channel, bssid)`), falls back to a normal scan on failure, and
  invalidates the cache after 2 failures.
- Bounded: overall connect timeout (constant, 8 s), and a non-blocking API the session loop can
  poll (`net::begin()`, `net::poll() -> Connecting|Connected|Failed`, `net::stop()`), plus
  `net::powerDown()` for sleep (Wi-Fi off, modem sleep released).
- Log: `NET: phase=…`, `NET: connect_ms`, `NET: cache=hit|miss|invalid`, RSSI, channel. Never log
  the password; the SSID only as its length + first 2 chars.
- Pure logic (cache record validity, fallback decisions, retry policy) lives in `lib/net`, tested natively.

## Files to touch
- src/net.h, src/net.cpp (new); lib/net/src/* (new); test/test_net/* (new)

## Acceptance criteria
- [ ] `pio test -e native` and the m5paper build pass; the cache and fallback policy is unit-tested.
- [ ] Code review: no blocking wait without a timeout; the password never logged.
- [ ] (Later, on device with the user's config) a first connect, then a reconnect with `cache=hit`
      and a clearly shorter `connect_ms`.

## Notes
- New files only; hooking into src/main.cpp comes with the session wiring task.
- Session 1 result (2026-10-28, uncommitted, not yet reviewed): `src/net.*` (non-blocking
  begin/poll/stop/powerDown), `lib/net` netpolicy (cache record in RTC + NVS, cached BSSID/channel
  attempt 3 s, then scan, total 8 s, invalidation after 2 failures), 14 native tests.
  Next: tester review, then commit; the device check needs the user's config (task 020).
  Known gap: a cached DHCP lease that conflicts on the LAN isn't detected.
- Review (2026-10-28): PASS (PC); device NOT VERIFIED. Fixes in progress: memcmp over struct padding
  (net.cpp:110), failures not persisted to NVS (power cycle resets the 2-failure rule),
  `net::invalidateLease()` for the HA client to call on unreachable host, ignored `WiFi.begin`
  return values. Watch on the device: Arduino's first_connect internal retry overlapping our
  fallback; `begin()` after Failed without powerDown.
- Review fixes done (2026-10-28): field-by-field `sameRecord`, failure count persisted to NVS (on change),
  `net::invalidateLease()` + 24 h lease age (RTC epoch), `WiFi.begin` return checked, wrong password
  logged as `auth_failed` without counting as a cache failure. 35 net tests. Device check pending.
