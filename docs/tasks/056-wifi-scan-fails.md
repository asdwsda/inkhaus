# 056: Wi-Fi scan always fails (aps=-2 after 6 s)

Status: done
Depends on: 047
UI spec: none
Needs device: yes

## Scope
Every cold boot in Auto with 2 profiles logs `NET: phase=scan profiles=2` then
`NET: scan_done aps=-2 found=0 pick=0 ms=6004` (WIFI_SCAN_FAILED, hits the 6 s budget); Auto only works
because it falls back to profile 0's cache. One earlier boot failed entirely (`attempt_scan`,
`failed connect_ms=8010`). Evidence: scratchpad visual.log t=5.2-11.2 (session 2026-10-30).
Find why the async scan (`WiFi.scanNetworks(true,false,false,300)`, src/net.cpp) fails (mode/STA not
started, scan started before `WiFi.mode`, polling the result wrongly, ...) and fix it.
Out of scope: profile selection logic (lib), unless the bug is there.

## Files to touch
- src/net.cpp (and lib net selection only if needed), tests

## Acceptance criteria
- [ ] Root cause in Notes.
- [ ] Device: cold boot in Auto logs `scan_done aps>0 found>=1` well under 6 s and connects to the
      strongest listed profile in range.
- [ ] Forced profile and the cached path unchanged (`cache=hit`, ~0.3 s).

## Notes

Phase 1 (PC, unverified on device): `-2` after 6004 ms is the arduino-esp32 library's own scan timeout:
`scanComplete()` returns WIFI_SCAN_FAILED once `millis()-_scanStarted > dwell*20`, and 300 ms * 20 = 6000 ms, the same
as our budget. So the scan never delivered SCAN_DONE (no result) rather than polling wrongly. Likely cause:
the `esp_wifi_scan_stop()` called just before `scanNetworks()` in startScan() (async message to the Wi-Fi task,
right after `WiFi.mode(WIFI_STA)`) aborts or races the scan that follows. Fix in src/net.cpp startScan():
removed the pre-stop, dwell 300 -> 200 ms (lib timeout 4 s, scan ~2.6 s), retry once if scanNetworks returns
FAILED (logs `NET: scan_start_failed retry`). finishScan's stop on a 6 s timeout is kept. Device check pending;
if aps=-2 persists, log `scanNetworks` return value and Arduino SCAN_DONE event arrival.
- 2026-10-30 device check 1 (check4.log in the session scratchpad): cold boot scan OK
  (`scan_done aps=9 found=1 pick=1 ms=2502`). After a deep-sleep touch wake it failed again:
  `scan_done aps=-2 found=0 pick=1 ms=4890`, then profile 1 cache hit (0.28 s). Open: (a) scan after a
  deep-sleep wake; (b) Auto scans on every wake (2.5 s+ awake per session) even though the last profile
  has a valid cache: try the last profile's cache first and scan only on failure (`fallback=scan`).
- 2026-10-30 follow-up (PC + cold-boot device check):
  (b) Cause: `netselect::profileWithoutScan` skipped the scan only for `last == 0` (047 intent: a higher-priority
  profile #1 might be back in range). Last connected was profile 1, so Auto scanned on every wake. Now any
  last-connected profile with a usable cache is tried first (no scan); a failed connect still takes the existing
  `direct_failed ... fallback=scan` path (scan once, exclude the failed profile). Trade-off: if #1 returns while
  #2's cache still works, Auto stays on #2 until it fails or the user picks #1. Tests updated (test_net).
  (a) Root cause of the post-deep-sleep scan failure NOT found by reading: no other Wi-Fi/PHY/sleep code touches
  the radio on that path, and `start_ret=-1` (RUNNING) shows the scan was accepted; only the SCAN_DONE event is
  missing. Added diagnostics to the log: `phase=scan ... start_ret=N` and `scan_done ... event_ms=N` (-1 = no
  SCAN_DONE event arrived). Cold boot (2 captures, hardware reset): `aps=13/11 found=1 ms~2400-2600 event_ms~=ms`.
  With (b) the post-wake scan no longer runs in the normal case, so the failure only matters after moving away.
  Next: deep-sleep touch wake with a forced scan (e.g. Auto after changing location, or clear the RTC last-profile)
  and read `event_ms`; if -1, suspect partial PHY calibration from RTC data after deep sleep.
- 2026-10-30 device check 2: cold boot `scan_done aps=12 found=1 ms=2702 event_ms=2694`; touch wake from
  deep sleep connects with no scan (`cache=hit`, 0.22 s, check6.log). Scan after a deep-sleep wake is
  still unexplained but now only runs when the direct connect fails; `event_ms` logs it if it recurs.
