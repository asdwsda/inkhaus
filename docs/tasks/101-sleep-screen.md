# 101: Sleep screen

Status: in-progress (PC part done 2026-11-03, device check pending)
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

## Notes (firmware-dev, PC part)
- Done: settings key `sleep_screen` (NVS `sleepscr`) + While asleep pip row on the Sleep page (rows/notes per settings.md 8.2); config `sleep_screen:` (weather/calendar/notifications, cache format 8); lib/sleep_screen (layout, texts, fallbacks); `persistent_notification/get` via HaWorker/SessionDriver (lib/ha_client/notifications); src/render/sleep_screen_draw; sleep entry in main.cpp goToSleep (GC16 full, no pill, scheduled wake via finishScheduled); wake: present flag `slpscr` makes the first dashboard frame one GC16 full; SHT30 is read on every wake when the setting is on (report_glue readAnyway); sleepwake::enter stores indicator None.
- Open (device): look check, `!` battery glyph, weather icon mapping (existing set), HA result shapes. Calendar entity attributes fetched via domain_attrs; widget `attribute:` extras are still not wired to HaClient (pre-existing: set_extra_attributes has no caller).
- 2026-11-03 tester review: PASS (evidence scratchpad review101.md). Open minor: the notifications WS call closes
  the keep-alive HTTPS socket, so later graph fetches pay a new TLS handshake; follow-up fix.
