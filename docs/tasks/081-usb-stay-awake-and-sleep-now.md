# 081: Stay awake on USB (setting) and a manual "sleep now"

Status: PC part done (device check pending)
Depends on: 035 (sleep), 080 not required
UI spec: docs/ui/settings.md section 9 (done)
Needs device: yes

## Scope
1. Setting "Stay awake on USB" (on/off, default off). When on and the device is on USB power, the idle timeout
   does not put it to sleep: the screen stays live (state updates keep coming over the kept-alive connection).
   Going back to battery (USB unplugged) restarts the normal idle timer; the device then sleeps as usual.
2. Manual sleep, usable in any mode (also on USB with the setting on):
   - Rocker: hold the push for >= 2.5 s -> sleep now. The existing long push (600 ms) keeps its meaning; when the
     hold passes 2.5 s it only sleeps (the 600 ms action must not run as well; trigger long push on release
     or cancel it).
   - Settings: a "Sleep now" row on the overview page.
   - Both use the normal sleep path (indicator, wake sources, scheduled refresh alarm unchanged).
- Out of scope: changing the idle timeout; charging display.

## USB detection (research first)
ARCHITECTURE/boot.cpp: no VBUS sense is known for this board; `M5.Power.isCharging()` only reports while
charging (likely false when the cell is full). Find a reliable signal, in this order:
- a VBUS/charger-status GPIO or ADC on the M5Paper schematic;
- M5Unified API returning a USB/external-power state for this board;
- fallback heuristic: isCharging, or battery_mv >= ~4250 and not falling over N minutes.
Record the chosen method in the task Notes; tag heuristics `UNVERIFIED` and add a line to docs/device-checks.md.
Pure decision logic ("stay awake?", hold-duration handling) goes in lib/ with native tests.

## Files to touch
- lib/settings/src/settings.{h,cpp}, lib/settings_ui/src/settings_ui.cpp (toggle + Sleep now row)
- src/settings_nvs.cpp (persist the toggle)
- lib/app_logic/ or lib/power/ (stay-awake decision, natively tested)
- src/input.cpp, lib/ui/src/ui_rocker.* (2.5 s hold event)
- src/main.cpp / src/sleep.cpp (idle timer uses the decision; manual sleep entry)
- test/

## Acceptance criteria
- [ ] Native tests: stay awake only when setting on AND usb powered; no refresh alarm while staying awake on USB; unplugging restarts the idle timer;
      2.5 s hold yields SleepNow and no LongPush; a short hold still yields LongPush.
- [ ] Setting saves and restores across sleep and power loss.
- [ ] Device, USB + setting on: no sleep after the idle timeout (> 2x the timeout); HA state changes still appear.
- [ ] Device, USB + setting on, unplug: sleeps after the normal idle timeout.
- [ ] Device: 2.5 s rocker hold sleeps from the main screen, a popup and Settings; "Sleep now" row sleeps.
- [ ] Setting off: behaviour unchanged from today.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
- User 2026-11-02: 2.5 s hold + "Sleep now" row approved; skip the hourly refresh alarm while staying awake on USB.
- Long push today: opens/leaves Settings, closes popups (app_state.cpp, settings_ui.cpp).
- PC part done. USB detection: M5Unified has no VBUS/charge signal for the M5Paper (isCharging/getVBUSVoltage
  documented as non-functioning), so `power::UsbDetector` (lib/power) infers USB from the battery voltage: full cell
  (superseded, see next note). UNVERIFIED (device-checks.md).
- Detector v2 (architect, after tester: fresh cell reads 4.39-4.41 V, above USB 4.32-4.35 V): wake starts "not USB";
  isCharging true => USB; else 2 min window of smoothed samples: drop <= 8 mV and >= 3900 mV => USB, drop >= 20 mV
  => battery, in between keeps state (max 60 min since the last flat verdict). Mid-session guard:
  `power::below_low_battery(smoothed mV)` while staying awake => `lowBatterySleep()` (task 079 swaps the limit there).
  PushHold kLongMs 600 -> 500 (M5 click limit), no 500-600 ms gap. UNVERIFIED.
- Idle hold: `power::StayAwakeGate` feeds the idle timers (g_drv->input / g_localIdle) once a second while setting
  on AND USB; no sleep happens, so no alarm. Hold: `ui::PushHold` (lib/ui): LongPush now fires on release
  (600 ms..2.5 s), `input::Event::SleepNow` at 2.5 s. Manual sleep = `manualSleep()` in main.cpp (goToSleep path).
