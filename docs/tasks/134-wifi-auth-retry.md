# 134: Wi-Fi auth failure on a weak signal retries

Status: done (device verified)
Depends on: none
UI spec: none
Needs device: no (device check later)

## Scope
Device log 2026-11-27 (rc1, RSSI -78): cached attempt ended `NET: auth_failed` after 205 ms; the wake went
offline. The next boot connected. `Connector::step` (lib/net/src/net_policy.cpp) treats AuthFailed as a wrong
password and fails at once, but handshake timeouts (reasons 15/204) on a weak link look the same.
- First AuthFailed of a wake: retry once (cached attempt -> scan attempt, cached attempt -> plain retry if
  already scanned) within the existing overall timeout.
- Second AuthFailed in the same wake: fail as now (wrong password).
- Out of scope: changing the status text or the cache invalidation rules.

## Files to touch
- lib/net/src/net_policy.{h,cpp}, src/net.cpp (if the retry needs a driver call), test/test_net*

## Acceptance criteria
- [ ] Native test: one AuthFailed then Connected -> Phase::Connected; `authFailed()` false.
- [ ] Native test: two AuthFailed -> Phase::Failed, `authFailed()` true.
- [ ] Retry respects the overall connect limit.
- [ ] `pio test -e native` and `pio run` pass; log line `NET: auth_failed retry=1`.

## Device result 2026-11-27 (fails, reopened)
Both first boots after a USB flash hit `NET: auth_failed` ~200 ms into the cached attempt (right after
`scan_done`); a plain reset afterwards connects fine. With the 134 retry: `auth_failed retry=1`,
`phase=attempt_scan`, then `begin_failed attempt=scan` at once, so the wake still fails.
Logs: assets/134-boot-after-flash-1.txt (no retry), assets/134-boot-after-flash.txt (retry).
Next: find why auth fails on the first boot after flash (cached attempt during/after the startup scan?
stale driver state, NVS Wi-Fi config?) and why `begin` fails right after an auth failure (needs a
disconnect / delay first?). Device needed: reproduce by flashing, then resetting.

## Device result 2 (2026-11-27)
Cause (measured): first boot after a flash scans, then the BSSID-directed cached attempt is rejected with
disconnect reason 202 (AUTH_FAIL, ~200 ms); plain reset boots connect. Not a wrong password or stale cache
(cached BSSID matches the scan; two APs share the SSID, ch6 -78 dBm cached, ch11 -62). The 134 retry then
fired, but `WiFi.begin` was refused right after the auth disconnect (driver busy): fixed by retrying begin up
to 4 times with 100 ms pauses (`NET: begin_refused try=N`). Log now `auth_failed retry=1 reason=N`.
Verified: 2 flash+boot cycles connected (retry path, connect_ms ~450): assets/134-flash-cycle-1.txt, -2.txt.
The AP-side reason for the 202 on the directed attempt is not found (left open).
