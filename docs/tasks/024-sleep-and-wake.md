# 024: Sleep, wake and scheduled refresh on the device

Status: in-progress
Depends on: 005, 016, 023
UI spec: docs/ui/STYLE.md (sleep indicator), layout.md (`sleepIndicatorRect`)
Needs device: yes (final checks with USB unplugged, on battery)

## Scope
Make the session end in real sleep, per ARCHITECTURE.md (power strategy, sleep indicator, credits
of the boot/refresh rules):
- **Sleep entry** when the session machine emits `Sleep{plan}`: `present::prepareSleep()` (ghost
  debt), draw the sleep indicator variant (touch vs button) into its own rect and refresh only
  that rect, `ntpAbort()`, `net::powerDown()`, `boot::endBurstRead()`, then:
  - `PowerOff` (wake mode Button / low battery): arm the BM8563 alarm if `rtc_alarm_at` is set,
    then release the main power hold (GPIO2 low, as M5Unified's power-off does for M5Paper).
    On USB power the board stays powered: fall back to ESP32 deep sleep with the rocker push
    (GPIO38, ext0) and the RTC alarm (the BM8563 INT pin as ext1 if it's wired to a GPIO; check
    the M5Paper schematic in M5Unified's sources) as wake sources, and log which path was taken.
  - `DeepSleep` (wake mode Touch): keep the GT911 powered, wake on its INT (GPIO36) plus the rocker
    push and the RTC alarm; keep the EPD controller warm if that is cheap, else power it down.
- **Wake:** map the reset reason / wake cause / RTC alarm flag to `power::WakeReason`, clear the
  sleep indicator on the first redraw, start the right session kind (Interactive vs
  ScheduledRefresh). A scheduled refresh does not initialise touch input and sleeps right after.
- **Low battery:** the low-battery screen (simple full redraw: "Battery low – please charge"),
  no RTC alarm.
- Log `SLEEP: mode=… alarm_at=… indicator=… wake=…` and `WAKE: reason=…`. Battery mV at sleep.
- Keep the 016 crash guard working across sleep (a deep-sleep wake is not a crash).
- Pure parts (wake-reason mapping, which indicator, sleep-path choice) in lib with tests.

## Files to touch
- new src/sleep.{h,cpp}, src/main.cpp (the Sleep command and wake handling), src/present.* (indicator
  draw helper if needed), src/render/screen.* (indicator), lib/power or lib/app_logic (pure parts), tests

## Acceptance criteria
- [ ] `pio test -e native` and the builds pass; pure logic tested.
- [ ] On device (later): Button mode on battery: the indicator appears, the device powers off, a rocker
      push wakes it into the same screen without a full flash; the RTC alarm wakes it for a scheduled
      refresh (use a short test interval build flag) and it sleeps again; Touch mode: a tap wakes it.
- [ ] On device (later): measured awake time per wake from the log.

## Notes
- PC-only until the device is back; the battery-only checks need the user to unplug USB.
- Progress (PC side done, device unverified): lib/app_logic/src/sleep_logic.* (wake mapping, indicator, sleep path,
  BM8563 alarm fields, WakeInputGuard) + test/test_sleep_logic; src/sleep.* (detect/enter); src/main.cpp (goToSleep,
  low-battery early path, scheduled = no input); src/present.* (showNotice); src/render/screen.* (indicator).
  Findings: M5Unified has NO GPIO2 power-off for the original M5Paper (Power_Class.inl:642-651 sets only wakeup pin 36,
  no rtcIntPin, no power_hold pin; _powerOff then just deep-sleeps, :1716-1783). GPIO2 = M5EPD_MAIN_PWR_PIN comes from
  M5GFX.cpp:1644. The BM8563 INT is not a GPIO in M5Unified, so the ESP32 timer stands in for it on USB.
- Next (device): see the check list in the architect report.
