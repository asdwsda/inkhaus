# 034: "Demo mode" setting

Status: in-progress
Depends on: 002, 007, 022, 033
UI spec: docs/ui/settings.md (revision with a 5th row, by ui-designer)
Needs device: yes for the final check (later)

## Scope
User request: switch to the built-in demo showcase at any time, even with secrets + config on the card.
- lib/settings: new field `demo_mode` (bool, default off, NVS key `demo`), same load/save/validation
  rules; tests.
- App: when `demo_mode` is on → load the embedded demo.yaml + demo states, never start Wi-Fi/HA
  sessions, status bar "Demo"; sleep/wake keep working (scheduled refresh does nothing but sleep).
  Turning it on/off (leaving Settings) reloads the config source and does a full redraw. Pure
  decision logic (which config source, whether to run sessions) in lib with tests.
- lib/settings_ui + src/render/settings_screen: the 5th row per the revised settings.md; cycling
  Off/On; "unsaved" chip rules as for the other rows.

## Files to touch
- lib/settings, lib/settings_ui, src/render/settings_screen.*, src/main.cpp, src/app_config.*,
  tests

## Acceptance criteria
- [ ] Native tests: settings field + defaults + old stores; settings_ui row (layout rects from the
      revised spec, cycling, unsaved); config-source decision.
- [ ] `pio test -e native` and the builds pass.
- [ ] On device (later): toggling demo mode switches between the showcase and the real dashboard;
      no Wi-Fi activity in demo mode (log).

## Notes
- Paused (2026-10-28, credits): logic part done (settings field, demo_mode.* decision logic, config source, reload on leave, row model; 498 tests). Open: 5th row layout/drawing after the settings.md revision; no unit test for demoChanged yet; plan_sleep still schedules refreshes in demo mode.
