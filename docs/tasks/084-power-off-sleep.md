# 084: Make full power-off sleep work on battery

Status: todo
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
