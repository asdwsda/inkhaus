# 083: Screen greys out while sleeping

Status: deferred (next release; user 2026-11-02)
Depends on: 035 (sleep), 082
UI spec: none
Needs device: yes

## Scope
User photo (assets/083-grey-drift.jpg, 2026-11-01): after hours on the table, everything except the regions redrawn
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
- 2026-11-02 (firmware-dev) step 1 findings. Source only (no schematic in the libs): M5GFX drives GPIO23 as the IT8951 *reset*
  (M5GFX.cpp:1639/1702; pulsed, never a power enable); GPIO2 = main power hold (stays high in deepsleep_hold, so the whole board incl.
  IT8951, TPS65185 and the panel supply stays powered); GPIO27 = BUSY. M5Unified has no EPD power pin for the M5Paper (the only EPD_PWR is
  the other board's PM1). The TPS65185 is driven by the IT8951 (its power-up/down sequence runs per refresh, rails off between
  refreshes in normal operation), so a bias that stays on in sleep would come from the IT8951 state: STANDBY (0x0002, clocks gated,
  memory kept) is what we use; SLEEP (0x0003) is the lowest state, only a hardware reset (GPIO23) wakes it, memory lost.
  A separate EPD power switch was not found in the libs (M5EPD's EXT_PWR_EN GPIO5 / EPD_PWR_EN GPIO23 are from memory, unverified): not used.
- Step 2 done: env `m5paper-epdoff` (-DEPD_SLEEP_OFF): boot::controllerSleep() = IT8951 SLEEP before deep sleep; wake/reset-button/power-on
  start with the stock hardware-reset path (a no-reset start hung 15 s on a sleeping controller: task_wdt, found on device), cold =
  bootFrame re-renders the memory from the record. Device (60 s timer wake, logs in scratchpad epdoff_run*.log): timer wake works,
  `epd=sleep`, display_start 1.07 s (normal build: 1.33 s, it waits 0.86 s BUSY too), t_first_draw 2.13 s vs 3.66 s (different diff work, 0 vs 2
  rects), so wake is not slower. Current draw in sleep NOT measured. Look test (4 h, photo) pending: user.
- Step 3 done: app_logic::needPeriodicFull (>= 3 h since the last GC16 full, scheduled wakes only, unknown = due); main.cpp keeps
  g_rtcLastFullEpoch (RTC_DATA) and passes it to present::prepareSleep(forceFull) before the sleep badge. Verified on device with the age
  temporarily at 50 s (`periodic_full=1`, `full_periodic` GC16 ~1.2 s).
- Switch back to the normal build: `pio run -e m5paper -t upload --upload-port /dev/ttyUSB0` (via tools/device_lock.sh).

- 2026-11-02 (user): the 4 h look test (epdoff vs. nofull) moves to the next release. The periodic GC16 full refresh (>= 3 h) stays in this release.
