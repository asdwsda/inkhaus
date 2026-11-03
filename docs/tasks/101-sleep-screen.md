# 101: Sleep screen

Status: in-progress
Depends on: 092, 100
UI spec: docs/ui/sleep-screen.md (mockups docs/ui/sleep-screen-*.svg, accepted 2026-11-03)
Needs device: yes (look check)

## Scope
Settings option "While asleep" (Dashboard | Sleep screen, default Dashboard = today's behavior).
With Sleep screen, the last frame before sleep is a dedicated screen instead of the dashboard with the pill:
- Large date and "updated HH:MM" (no live clock: e-ink only changes on wakes, so a clock would lie),
  next update time, battery, indoor temperature/humidity from the SHT30 (task 100).
- Optional blocks, shown only when configured in config.yaml (`sleep_screen:` with `weather:` entity,
  `calendar:` entity, `notifications: true`); the layout must look complete with none of them.
- Wake (tap/rocker) shows the dashboard tab that was active (task 086 tab restore), no extra flash beyond
  the normal wake refresh.
- Refresh: scheduled wakes redraw it as the GC16 full (task 092 rule); no change in power policy.
- Out of scope: images/greys beyond STYLE.md, weather icons beyond the existing icon set.

## Files to touch
- docs/ui/sleep-screen.md + mockups (ui-designer)
- lib/ (layout/compose logic, config parsing of `sleep_screen:`), test/
- src/ (sleep entry path, render), lib/settings (new key)

## Acceptance criteria
- [ ] Spec with rendered + checked mockups, portrait and landscape, with and without optional blocks.
- [ ] Native tests: config parsing, which blocks show, text fallbacks (no SHT30 reading, no weather state).
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] Device check: sleep screen shown at sleep, wake returns to the tab, scheduled wake updates it.

## Notes
- User 2026-11-03: footer "Asleep" + wake hint replaces the sleep pill (OK). Notifications block: HA persistent
  notifications are no longer entities, so fetch them with one short WebSocket call
  (`persistent_notification/get`, reusing the stats WS client), only when the block is enabled.
- User 2026-11-03: date as English weekday on its own line, no year (OK).
