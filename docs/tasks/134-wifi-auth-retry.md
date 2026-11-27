# 134: Wi-Fi auth failure on a weak signal retries

Status: PC part done (device check pending)
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
