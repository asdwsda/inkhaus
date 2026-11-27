# 135: Battery percent and low-battery shutdown from resting voltage

Status: todo (implement after ~3 days of debug logs, from 2026-11-27)
Depends on: none
UI spec: none
Needs device: yes (logs)

## Scope
HA history (2026-11-04..27, fw 1.4.0): steady ~0.8 %/h (~5 days per charge), but reporting stopped at
20 % (after 35 % one hour earlier) and at 32 %, for 82 h and 53 h: looks like the low-battery session
(power off, rocker-only, no alarm) triggered early. Suspects: voltage sag under Wi-Fi load, or the learned
limit (task 079, `batterylimit::limit`) drifting up. Last charge fell 72 -> 49 -> 4 % within ~17 h:
the voltage-to-% mapping is poor at the low end.
1. Analyse the new wakelog.csv (battery_mv per wake, low_battery flag, learned limit) to confirm the cause.
2. Judge low battery on a resting reading (before Wi-Fi / display, several samples), and log the learned
   limit at every low-battery decision.
3. Replace the linear %-from-mV with a Li-ion curve calibrated from the logs.

## Files to touch
- src/app_core.cpp, lib/power (battery limit, % curve), lib/app_logic/src/sleep_logic.*, tests

## Acceptance criteria
- [ ] Cause of the early shutdowns stated with log evidence in Notes.
- [ ] Native tests: a single sag reading below the limit does not start the low-battery session.
- [ ] Native tests: % curve monotonic, 0 % at the limit, 100 % at full charge.
- [ ] Device: one full charge logged with no low-battery shutdown above ~10 %.

## Notes
- 2026-11-27 user: the past weeks ran Sleep mode = Deep sleep (rocker wake), updates every 6 h. Still only
  ~5 days per charge. The sleep log says `path=deepsleep_hold epd=standby touch=awake` in this mode: the GT911
  stays powered though touch can't wake it. Check standby current with touch asleep (see 035) and the
  Wi-Fi time per wake (weak AP, task 137).
- 2026-11-27: battery test started. Device unplugged on fw 1.5.0-rc2 code (debug build, reports 1.5.0-rc1 debug,
  commit after 139), Debug log on, Sleep mode Deep sleep, updates 6 h, picks the ch11 AP (137). GT911 stays
  awake in sleep (139: its sleep can't be undone without an EN reset). On return (~2026-11-30): plug in, pull
  wakelog.csv + debug.log (tools/wakelog.py, wake the device first), check 131/132/138 lines, then do this task.
