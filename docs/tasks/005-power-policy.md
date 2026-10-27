# 005: Power policy (logic only)

Status: done
Depends on: 002
UI spec: none
Needs device: no

## Scope
Pure decision logic for what the device does on each wake and how it sleeps.
Hardware wiring (power-hold line, RTC alarm, GT911 wake) is a later task.

`lib/power`:
- Inputs: wake reason (`ColdBoot`, `Button`, `RtcAlarm`, `Touch`, `Unknown`),
  `Settings` (from `lib/settings`), battery voltage in mV, current time (epoch s),
  and the time of the last successful scheduled refresh.
- `decide_session(...)` → `Interactive` | `ScheduledRefresh` | `LowBattery`.
  - Battery below the low threshold (constant, default 3300 mV) → `LowBattery`
    (the app will show a "please charge" screen and stop scheduled refreshes).
  - `RtcAlarm` → `ScheduledRefresh`; everything else → `Interactive`.
- `IdleTimer`: fed with input events and "busy" flags (pending HA command, network
  in progress); says when to sleep: `idle_timeout_s` after the last input and only
  when not busy, with a hard cap (constant, 120 s) even when busy.
- `plan_sleep(...)` → `{mode: PowerOff | DeepSleep, rtc_alarm_at: optional epoch,
  touch_wake: bool, button_wake: bool}`:
  - `wake_mode=Button` → PowerOff, button wake, no touch wake.
  - `wake_mode=Touch` → DeepSleep, touch + button wake.
  - `refresh_interval_min=0` or LowBattery → no RTC alarm.
  - Otherwise the alarm is aligned to the interval boundary (e.g. 60 min → next full
    hour), and at least 60 s in the future.
- Battery percentage from voltage (simple piecewise-linear Li-ion curve) for display.

Out of scope: any hardware access, quiet hours.

## Files to touch
- lib/power/src/*
- test/test_power/*

## Acceptance criteria
- [ ] Session decision table covered by tests (each wake reason × normal/low battery).
- [ ] Idle timer: sleeps after timeout; busy delays sleep; hard cap applies (tests with fake time).
- [ ] Sleep plan for both wake modes and refresh off/on (tests).
- [ ] Alarm alignment: 15/30/60/120/240 min boundaries, and the ≥60 s rule near a boundary (tests).
- [ ] Battery percentage is monotonic and clamped 0..100 (test).
- [ ] `lib/power` has no Arduino includes; `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Result: `plan_sleep` takes a UTC offset; alarms align to local-midnight-based
  boundaries (all allowed intervals divide 24 h). Hard cap = max(120 s, idle timeout),
  measured from the last input.
- For the integration task: feed `IdleTimer` a monotonic clock (`millis()`), not
  wall time; battery curve points need tuning on the real device.
