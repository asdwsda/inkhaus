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
- PC part done (2026-10-28): PowerOff plans → deep sleep with GPIO2 hold, IT8951 STANDBY (0x0002, keeps memory; wake = SYS_RUN in probeController, fallback stock). GT911 left awake: its wake needs the host to drive INT (GPIO36, input-only) → risk of a dead touch panel; its standby current is the open cost, decided by the battery test. `BOOT: charging=` logged. SLEEP_POWER_OFF flag keeps the old path. 500 tests.
- Device evidence (2026-10-29, SD wake log, Touch mode): on USB a touch wake is DEEPSLEEP/EXT0 with rtc_boot_count 2 (deep sleep works). On battery every wake is POWERON with rtc_boot_count back to 1 and slept_path none → the board loses power during deep sleep: the GPIO2 hold does not survive. Next: hold GPIO2 as an RTC IO (rtc_gpio_init/output/level 1/rtc_gpio_hold_en) and keep ESP_PD_DOMAIN_RTC_PERIPH on.
- Research (2026-10-29, sources only, nothing measured): can the GT911 sleep in Button-mode deep sleep and wake reliably?
  - Pins (M5GFX.cpp ~1705, our boot.cpp:190): I2C SDA 21 / SCL 22 (port 1), INT = GPIO36, **no RST pin configured** (`pin_rst` stays -1; Touch_GT911::init only pulses RST if >= 0). M5Unified/M5GFX name no RST for the M5Paper; whether the board routes GT911 RST to any ESP32 GPIO is not in the sources (schematic not available here: UNVERIFIED, check the M5Paper schematic PDF).
  - Sleep: `Touch_GT911::sleep()` drives INT low, then writes 0x8040 = 0x05 (screen-off command). Wake (`wakeup()`): INT low 58 ms, high 2 ms, back to input. Both need the host to **drive INT**, but GPIO36 is input-only (SENSOR_VP): `pinMode(36, OUTPUT)` does nothing. So the library calls are half-ineffective here: the sleep command is accepted over I2C, but the INT edge for the address select / wake can't be produced. Per the GT911 datasheet a sleeping chip is woken by an INT high pulse (or a reset); a touch alone does not wake it. Expect a dead touch panel after sleep unless RST is reachable (reset re-runs address selection with INT, so even then INT state matters; 0x5D vs 0x28/0x29 depends on INT at reset).
  - I2C wake command: none documented for GT911 (it ignores I2C while in sleep mode, no ack), so no software-only wake. Hence the current "GT911 left awake" decision is correct for Button mode.
  - Power-cycling via a rail: M5EPD pins are MAIN_PWR GPIO2 (whole board), EXT_PWR GPIO5 (5 V boost for the grove port), EPD_PWR GPIO23 (here used as IT8951 reset). None is known to switch the GT911's 3V3; it sits on the always-on rail (UNVERIFIED). So only the whole-board power-off (the GPIO2 path that doesn't survive on battery) cuts it.
  - Wake in Button mode needs no touch at all (rocker ext1 on the GPIO37/38/39 pins), so the GT911 is only a load. After a wake from deep sleep the touch is re-initialised (boot.cpp), which also resets its state.
  - Expected saving, from the GT911 datasheet (typical, UNVERIFIED on this board): normal/green mode roughly 1-3 mA average with INT idle, sleep mode about 0.1 mA; so at most ~1-3 mA of the standby budget, against a total that the battery test (this task) measures. At a 1 month target on ~1150 mAh (~1.6 mA average) that is the same order as the whole budget, so worth measuring: compare standby current with the touch awake vs. the I2C sleep command sent (the command alone is harmless if we always re-init on wake with a reset-free path, but then wake after a *touch* mode sleep must stay awake).
  - Option to test on the device: send only the 0x8040=0x05 command (skip the INT drive), measure the standby current; on wake, read the I2C address: if it never answers, the only recovery is a power cycle, so in Button mode (where touch is not needed to wake) a failed re-init could be tolerated once the rocker is used. Needs a timed device run; risk is a dead touch panel until the next full power loss/reset. Recommendation: measure first; adopt only if the saving is > ~0.5 mA and the GT911 answers after the wake (reset button as the recovery).
- Battery test plan (user, 2026-10-30): data collection starts with v1.2.0. Flash it, turn on the debug log (Settings, Tools), charge to full, unplug and use normally. Each wake logs SLEEP battery_mv and awake_ms to the SD wakelog. Read it out later over USB (the `wakelog` serial command) and analyze: mV per day, wakes per day, mean awake ms.
- Confirmed by the user (2026-10-30): the battery test starts right after the v1.2.0 install, with the debug log on.
