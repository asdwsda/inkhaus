# 137: Prefer the clearly stronger access point over the cached one

Status: done (device verified 2026-11-27, assets/137-pick-ch11.txt)
Depends on: 134
UI spec: none
Needs device: yes (verify), PC part first

## Scope
The user's SSID has two APs: cached ch6 at -78 dBm, the other ch11 at -62 dBm (task 134 logs,
docs/tasks/assets/134-flash-cycle-*.txt). The cache keeps the device on the weak AP: slower wakes, AUTH_FAIL
202 after flash, more battery.
- When a scan result is available in this wake (scan already done) and another BSSID of the same profile is
  >= 8 dB stronger than the cached one, connect to the stronger one and update the cache.
- Wakes without a scan keep the cached fast path (no extra scan just for this).
- On a cached connect with RSSI < -75 dBm, mark the cache so the next wake scans once (cheap re-evaluation),
  at most once per 24 h.
- Out of scope: roaming while awake.

## Files to touch
- lib/net/src/net_policy.{h,cpp}, src/net.cpp, test/test_net

## Acceptance criteria
- [ ] Native tests: stronger BSSID (>= 8 dB) wins over cache; < 8 dB keeps cache; weak cached connect
      sets the rescan flag; rescan at most once per 24 h.
- [ ] `pio test -e native`, `pio run` pass.
- [ ] Device: after a scan wake, log shows the ch11 AP picked (`NET: pick bssid_rssi=-62 ...`).
