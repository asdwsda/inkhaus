# 035: Button mode sleeps in deep sleep (power hold kept), power-off behind a flag

Status: in-progress
Depends on: 024
UI spec: none
Needs device: yes (on battery, later)

## Scope
User report: on battery the device doesn't wake after going to sleep (rocker, long press and the back
reset button all do nothing); on USB everything works. The GPIO2-low power-off path is the likely cause
(unverified assumption from task 024). Per ARCHITECTURE.md (2026-10-28):
- Default for `PowerOff` plans (Button mode, low battery): ESP32 deep sleep, GPIO2 held high
  (`gpio_hold_en` + `gpio_deep_sleep_hold_en`, as touch mode), wake on the rocker push (ext1 GPIO38)
  and the ESP32 timer for `rtc_alarm_at` (and the BM8563 alarm as before, harmless).
- Before sleeping: put the IT8951 into standby (the IT8951 command set's standby/sleep; prefer the
  mode that keeps the image memory, so the next wake is "warm") and the GT911 into its sleep mode
  (Button mode only; Touch mode needs it awake). Bounded waits, `endBurstRead()` last. On wake, bring
  both back (IT8951 SYS_RUN / the existing start-up path, GT911 wake via INT/reset per its datasheet;
  M5Unified/M5GFX may already have helpers, check `Touch_GT911` and `Panel_IT8951`).
- `-DSLEEP_POWER_OFF` keeps the old GPIO2-low path for testing.
- Logs: `SLEEP: path=deepsleep_hold epd=standby touch=sleep`.
- Native tests for any changed pure logic (plan path selection).

## Files to touch
- src/sleep.cpp/.h, src/boot.cpp (wake from standby), lib/app_logic sleep_logic (+ tests)

## Acceptance criteria
- [ ] `pio test -e native` and the builds pass (with and without SLEEP_POWER_OFF).
- [ ] On device (later, on battery): Button mode sleeps, rocker wakes into the same screen;
      scheduled refresh wakes; standby current measured (battery test plan).

## Notes
- Also prepare: a short "is the battery OK" check — after boot on battery, log `BOOT: on_battery=1`
  (VBUS sense if available on this board, else infer from battery voltage trend) so the next test
  shows it directly.
- User test (2026-10-28): unplugged while awake → kept working on battery. So the battery is OK; the failure is waking from the GPIO2-low power-off (rocker and reset do nothing without power). Confirms this task’s approach; SLEEP_POWER_OFF stays a test-only flag.
