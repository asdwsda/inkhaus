# 093: Task watchdog restart on a scheduled wake

Status: PC done (device check pending: recovery path untested)
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


2026-11-02 tester (code review only, no device): PASS with caveats.
- False-reset risk: none found. HRDY (BUSY pin) is only low while the IT8951 processes a command/transfer; the panel refresh itself runs on the LUT engine and is polled via LUTAFSR (Panel_IT8951.inl:_check_afsr, 3 s bound), not via the pin. Every display() is followed by waitAndCheck (present.cpp:157, 348/371/394 callers too), so guardController (present.cpp:142) at refresh()/pushRegion() entry runs with the engine idle and the pin high; the 450 ms / 1.2 s / 430 ms refresh durations sit inside waitAndCheck, never under the 300 ms guard. Residual: HRDY right after the last data burst of a whole-frame push before refresh() (present.cpp:253/467); that is command processing (ms), well below 300 ms. Unmeasured on device: confirm no `busy_timeout=push|full` in a normal wake log.
- Normal path latency: guard = one digitalRead + immediate return (boot.cpp:257-264, bounded_wait.h); no delay unless BUSY low.
- WDT: waitBusyHigh feeds each poll (boot.cpp:261); init(true) gets fed before/after (boot.cpp:313,318). Worst recovery = 300 + 300 (ensureControllerReady re-waits, boot.cpp:310) + init(true) library waits (several 4 s) -> can approach but likely stay under 15 s; not proven. esp_task_wdt_reset after setupDone just returns an error, harmless.
- Loop: no loop; recovery is one pass per guard call. A permanently dead controller costs ~0.6 s + init per refresh/push and sets g_forceFull each time (fullRefresh clears it at :242, guard re-sets it), so every frame stays full; bounded, no hang.
- Gaps: guard checks only the entry; a stall inside the library's whole-frame write or d.display() itself, and the stock-path init in M5.begin, remain unfed (as the Notes say). Minor: ensureControllerReady's second 300 ms wait is redundant after a failed guard.
- 2026-11-02 (architect): seen again on the 092 debug build (cap2.txt, after interactive sleep on USB, RTC wake hung at dbg_before_panel_init until TASK_WDT) — the library-init gap left open by ef9ac9d is real.
- 2026-11-02 firmware-dev: hang location = src/boot.cpp attachNoResetDisplay, `g_parts->panel.init(false)` (Panel_IT8951::init: ~8 commands, each `_wait_busy` up to 4 s unfed; cap2.txt: pre_init BUSY check passed, then no further log until TASK_WDT 14 s later; happened after a USB deepsleep wake with STANDBY->SYS_RUN). Cause of the stuck BUSY inside init not reproduced. Fix: WDT window raised to 60 s only around the library init (`esp_task_wdt_init`), then restored; if init took > 2 s (`BOOT: panel_init_slow ms= busy=`), redo it with the hardware reset (init(true), treated cold, full redraw). Device: m5paper-debug + temp 90 s alarm + temp `sleepnow` serial cmd (reverted): 12 scheduled wakes in 3 rounds (each round: reset, sleepnow on USB, 4 chained wakes), all reset_reason=deepsleep, no TASK_WDT, no panel_init_slow (the hang itself did not recur, so the recovery path is untested on device). Log: scratchpad wakeloop.log.
