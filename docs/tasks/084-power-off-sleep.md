# 084: Make full power-off sleep work on battery

Status: PC part done (battery test pending)
Depends on: 035
UI spec: none
Needs device: yes (user unplugs USB and presses the rocker; results read from the SD wake log afterwards)

## Scope
Today every sleep is `deepsleep_hold` (GPIO2 high): ESP32 deep sleep with the IT8951 in standby, the GT911
awake, panel power and SD powered. Measured 2026-10-30..11-02: ~67 h per charge (est. 6-11 mA at rest).
Full power-off (GPIO2 low) is behind `-DSLEEP_POWER_OFF`; on 10-28 the device on battery did not come back after
a power-off (rocker and reset didn't help). Goal: make power-off reliable, because it is the only way to
the ~1 month target.

1. Find why it didn't wake: what powers the board on (rocker push via the power latch? BM8563 INT?), whether
   the RTC alarm/INT state or a held GPIO blocked the next power-on, whether the battery was simply flat.
   Check the M5Paper schematic and M5EPD's shutdown()/shutdown(seconds) sequence and copy what it does.
2. Wake sources from power-off, each tested on battery (USB unplugged): rocker push; BM8563 alarm
   (scheduled update); also check that the reset button brings it back if it ever hangs.
3. USB present: power-off leaves the board running (existing `deepsleep_fallback` path). Expose this as a
   reliable "on USB" signal at sleep time (`power::usbAtSleep()`), for 085's "Keep awake: On USB".
4. Log `slept_path=poweroff` and the wake cause in the wake log so the user's test can be read later.
- Out of scope: the Settings UI (085).

## Files to touch
- src/sleep.cpp, src/boot.cpp, lib/app_logic/src/sleep_logic.* (SleepExec), platformio.ini (if a test env)
- test/

## Acceptance criteria
- [ ] Notes: what powers the board on, and why the 10-28 attempt failed (evidence or best explanation).
- [ ] On battery: power-off, then rocker push wakes it, 10 out of 10 (user test, read from the wake log).
- [ ] On battery: power-off with an RTC alarm 5 min ahead wakes by itself and does the scheduled update, 3/3.
- [ ] On USB: power-off attempt falls back and reports USB; the device keeps working.
- [ ] Wake from power-off to a usable screen: time recorded (cold IT8951 start, ~1.1 s expected).
- [ ] `pio run` and `pio test -e native` pass.

## Notes
- User 2026-11-02: battery life on battery is the top priority; deep sleep with everything powered is not acceptable
  as the only option.
- 2026-11-02 PC part done (no schematic or M5EPD source available offline; the following is best explanation, UNVERIFIED
  until the battery test):
  - Power-on sources: M5Paper latches its main rail with GPIO2 (M5EPD `M5EPD_MAIN_PWR_PIN`, high = on). From off, the rocker
    push (its line is also a power-on source, besides GPIO38) and the BM8563 INT (alarm) switch the rail on again; M5EPD
    `shutdown(seconds)` = clear IRQ, set alarm, short delay, GPIO2 low. Our order was the same.
  - Likely causes of the 10-28 failure, both fixed in sleep.cpp: (1) a pushed rocker is a power-on source, so GPIO2 low while it
    was still down kept the board up, and the old 400 ms "still running = USB" check then re-asserted GPIO2 and fell into the
    deep-sleep fallback or half-collapsed rail; now: wait for rocker release (max 3 s) first, wait 700 ms for rail decay;
    (2) the sleep path was kept only in RTC memory, which a power-off loses, so the log could not tell; now it is in NVS
    (`slp/sp`) and the WAKE line prints `slept=poweroff`. A flat battery is not excluded (no voltage logged from that run).
  - USB signal: `power::usbAtSleep()` (lib/power, tested) is set when GPIO2 low leaves the board running; kept across deep sleep
    in an RTC var, false when unknown. Checked on USB 2026-11-02: `path=poweroff` -> after 700 ms `path=deepsleep_fallback`,
    device keeps working (serial log: scratchpad cap1.txt).
  - 081 item: `usb_rx=<0|1>` (GPIO3 pull-down probe, 3 ms, pull-up restored) is in the WAKE line (serial + debug.log) and
    in a `SLEEP: usb_rx=` line just before the power-off. On USB: usb_rx=1 at wake and at sleep (verified). Battery value: pending.
  - Wake time: WAKE line has `t_ms` (since chip reset, includes cold IT8951 start); wakelog.csv has display_ms etc.
  - Test env `m5paper-poweroff`: -DSLEEP_POWER_OFF -DWAKE_LOG_SD -DTEST_POWEROFF_ALARM_S=180 (always power off, rocker wake,
    RTC alarm ~3-4 min ahead, whatever the settings say). Flashed. Remove/keep as test env afterwards.
  - Battery test (user): see report; results: `tools/wakelog.py`, count `reason=button`/`rtc_alarm` after `slept_path=poweroff`.
  - Open: acceptance 2-5 need the battery test; if the board does not come back, run with the reset button and note.
