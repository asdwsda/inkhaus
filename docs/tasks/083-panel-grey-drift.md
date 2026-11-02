# 083: Screen greys out while sleeping

Status: todo
Depends on: 035 (sleep), 082
UI spec: none
Needs device: yes

## Scope
User photo (20261101_202055.jpg, 2026-11-01): after hours on the table, everything except the regions redrawn
by the last scheduled wake (graph) drifted to a dark grey background. Sleep path is `deepsleep_hold`: GPIO2 stays
high, IT8951 in STANDBY (sleep.cpp ~192). E-ink normally keeps its image unpowered for days, so the drift
suggests the panel/driver is still biased during sleep — which may also be part of the high sleep current
(est. 6-11 mA, task 079 data).

1. Investigate (cheap, first):
   - Is the EPD supply (IT8951 / panel power rails, e.g. GPIO23 EPD power enable on M5Paper) still on in sleep?
   - A/B, same screen after a GC16 full refresh, left 4 h on battery: (a) today's STANDBY, (b) IT8951 SLEEP
     and/or EPD power off before deep sleep. Compare the look (photo) and the mV drop.
   - Note: IT8951 SLEEP/power-off loses its image memory -> wake needs a full redraw (check wake latency).
2. Fix, by evidence:
   - If (b) stops the drift without breaking wake: use it (likely also saves sleep current).
   - Otherwise/also: on a scheduled wake, do one GC16 full refresh instead of the partial one when the last full
     refresh is >= 3 h old (constant). Cost: ~1 s extra display time every 3 h, well under 1 % of the
     battery used in that time.
- Out of scope: waveform tuning.

## Files to touch
- src/sleep.cpp, src/boot.cpp (EPD power/standby on sleep and wake)
- lib/app_logic/ (full-refresh-age rule, natively tested), src/present.cpp
- test/

## Acceptance criteria
- [ ] Notes record the A/B result (photos by the user, mV/h from the wake log).
- [ ] After 8 h unattended (hourly refresh), the screen has a white background with no visible grey drift.
- [ ] Native test: scheduled wake picks a full refresh when the last full refresh is >= 3 h old, else partial.
- [ ] Wake to usable screen is not slower than today by more than ~0.5 s (boot log), or the user accepts it.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
