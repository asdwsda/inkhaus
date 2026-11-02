# 093: Task watchdog restart on a scheduled wake

Status: todo
Depends on: 083, 091
UI spec: none
Needs device: yes

## Scope
Wake log 2026-11-02 17:00:40 (m5paper-epdoff build, Sleep mode Power off, Updates 30 min): reset_reason=TASK_WDT inside
setup; the 16:30 rtc_alarm wake was fine. 083 Notes already saw a 15 s task_wdt hang when starting without the IT8951
hardware reset on a SLEEPing controller. Find where it hangs (likely BUSY wait on the IT8951 after an epdoff sleep, or Wi-Fi/
history on battery), whether the normal build can hit it, and fix (bounded waits, no 15 s blocking without feeding the WDT).

## Files to touch
- src/boot.cpp, src/main.cpp, src/sleep.cpp (as found), test/

## Acceptance criteria
- [ ] Notes: hang location (file:line) with evidence.
- [ ] No blocking wait in the wake path can exceed the task WDT; a stuck IT8951 ends in a logged error + recovery, not a WDT reset.
- [ ] Device: 10 scheduled wakes in a row (short interval) on the affected build without TASK_WDT in the wake log.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
PC part done 2026-11-02 (UNVERIFIED on device; root cause by code reading, not reproduced). Evidence: wakelog 17:00 row reset=TASK_WDT in setup
(WDT armed at boot.cpp begin(), ended by boot::setupDone()). Candidate hang points (all library calls with 4 s BUSY waits and no feed):
- src/present.cpp pushRegion (d.pushGrayscaleImage/pushSprite, whole-frame write in bootFrame) and `d.getPanel()->init(false)` right after: a BUSY-stuck IT8951 (e.g. after an epdoff SLEEP) makes each command wait 4 s, a frame write has many -> >15 s.
- src/present.cpp refresh() `d.display()` (library waits before the update), then waitAndCheck (feed only after the wait).
- src/boot.cpp ensureControllerReady -> p->init(true) (several 4 s waits; now fed first); boot.cpp M5.Display.init_without_reset / M5.begin stock path (library waits, can't be fed from here).
- src/boot.cpp waitBusyHigh polled without feeding (300 ms bound, harmless alone).
Changes: lib/screen_track/src/bounded_wait.h (+ test/test_bounded_wait), boot::waitBusyHigh feeds while polling, present::guardController (300 ms BUSY check, logged busy_timeout/controller_recover, g_forceFull) before every refresh() and pushRegion(), feed before init(true). Not covered: stock-path init inside M5.begin. Device check line added (docs/device-checks.md, 093).

