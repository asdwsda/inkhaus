# 079: Learn the battery limits from the wake stats

Status: testing
Depends on: 029 (wake log), 035 (sleep)
UI spec: none (one Settings toggle row, same style as the existing on/off rows)
Needs device: yes (final check: a full discharge on battery)

## Scope
On 10-30..11-02 the device browned out at about 3.75-3.86 V (6 BROWNOUT resets, 1 TASK_WDT), and the battery
died at 3726 mV. `kLowBatteryMv` = 3300 never triggered, so there was no warning and no clean low-battery sleep.
The real cut-off depends on the cell and its age, so the device should learn it.

- Learning: when the device boots with reset reason BROWNOUT (or TASK_WDT/PANIC while on battery), record the last
  battery_mv seen before it. From these records, derive a learned low-battery limit = recent brownout voltage + a
  safety margin, and a learned "battery empty" voltage for the percent scale.
- Bounds: the learned limit stays within [3300, 3900] mV. It only moves up from a single event; it moves down slowly
  (e.g. after N full sessions below it without a brownout), so one bad reading can't make it unsafe.
- Persistence in NVS (survives power loss); a factory reset clears it.
- Setting "Learn battery limits" (on/off, default on). Off = fixed defaults (3300 mV, current percent curve).
  Learned values are kept when it's switched off, so turning it on again restores them.
- Optional: show the learned limit in the Settings battery/info row (read-only).
- Out of scope: measuring current; Wi-Fi start-up current spike (see 080).

Pure logic (record events, compute the limit, bounds, decay) goes in lib/ so it can be tested natively.

## Files to touch
- lib/power/src/ (new battery_learn.{h,cpp} or similar; power.cpp uses the learned limit instead of kLowBatteryMv)
- lib/settings/src/settings.{h,cpp}, lib/settings_ui/src/settings_ui.cpp (toggle)
- src/settings_nvs.cpp (or a new src/battery_learn_nvs.cpp) for persistence
- src/boot.cpp / src/main.cpp (feed reset reason + last battery_mv)
- lib/ui_state/src/widget_view.cpp, settings_ui.cpp (percent scale uses the learned empty voltage)
- test/ (native tests)

## Acceptance criteria
- [ ] Native tests: a brownout at 3780 mV gives a limit of 3780 + margin; repeated events take the max of recent ones;
      the limit never leaves [3300, 3900]; slow decay works; learning off gives 3300.
- [ ] Last battery_mv before a reset is known at the next boot (RTC memory or NVS), including after a brownout.
- [ ] Settings toggle saves and restores; switching it off uses the defaults without deleting learned values.
- [ ] Session selection uses the learned limit: below it -> LowBattery session (clean sleep, low-battery indicator).
- [ ] boot log prints `battery_limit_mv=<n> learned=<0|1>`.
- [ ] `pio run` and `pio test -e native` pass.
- [ ] Device: on battery, the device reaches a clean low-battery sleep instead of a BROWNOUT reset (checked in the
      wake log after a discharge; may take a few days).

## Notes
- Data: scratchpad wakelog of 2026-11-02 (brownouts at 3864, 3894, 3852, 3780, 3746 mV; last row 3726 mV).
- Architect consult before implementation (power + persistence): margin size, decay rule, where to keep the
  "last mV" so it survives a brownout, and whether the percent curve should change too.
- Decisions (architect, 2026-11-02; user approved defaults):
  - Setting default on. Learned limit is shown read-only in Settings (battery/info row).
  - Limit = max(brownout mV of events in the last 3 charge cycles) + 100 mV, clamped to [3300, 3900].
    No events in a cycle: limit -= 20 mV per charge cycle (floor 3300). A charge cycle = battery seen >= 4150 mV
    after having been below 3900 mV.
  - "Last mV" must survive a brownout: keep it in RTC_NOINIT and also in NVS, written at most once per wake and
    only when it dropped >= 20 mV below the stored value and is < 4000 mV (limits flash wear). Tag the RTC part
    UNVERIFIED (unknown whether RTC memory survives a brownout on this board).
  - Events: reset reason BROWNOUT, or TASK_WDT/PANIC with last mV < 3950 (not on USB). Keep the last 8.
  - Check the limit also right before Wi-Fi starts in each session (that's when the brownouts hit), not only at boot.
  - Percent scale: 0 % = learned limit (only when learning is on).
- Revised after review (architect, 2026-11-02): wake-log replay showed brownouts from 3924 down to 3726 mV in one
  discharge; max+100 pins the limit at 3900 (half the capacity lost) and it can't learn downward.
  - Limit = mean(event mV in the current and previous charge cycle) + 50 mV, clamped [3300, 3900].
  - Eventless charge cycle: limit -= 50 mV (probes down; new brownouts raise it again).
  - A charge cycle ends only after a real discharge (a `last_mv` < 3900 was stored) and >= 4150 mV seen on two
    readings >= 5 min apart (a short USB plug-in must not count).
  - TASK_WDT/PANIC count only if the reading at boot is not above last mV + 30 (else USB is inferred).
  - No factory-reset path exists in the firmware; the "factory reset clears it" item is dropped (namespace
    `batlearn` must be cleared if one is added later).
