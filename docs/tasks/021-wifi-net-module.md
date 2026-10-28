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
