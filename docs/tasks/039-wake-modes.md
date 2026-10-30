# 039: Three wake modes, wake-mode bug fixes

Status: done
Depends on: 035, 034
UI spec: docs/ui/settings.md (wake mode row: 3 values; drop the warn icon next to the value)
Needs device: yes (later)

## Scope
- lib/settings: WakeMode {Button, Touch, TouchAndButton} (NVS value 2 = TouchAndButton; old stores: 0 stays
  Button, 1 = old "Touch" → migrate to TouchAndButton so behaviour doesn't change), allowed values in display
  order Button, Touch, Touch & Button; tests.
- lib/power plan_sleep + lib/app_logic sleep_logic: wake sources per mode (Button: rocker only; Touch: touch
  only; Touch & Button: both) + RTC/timer as before; indicator variant per mode (Button → side-button variant,
  Touch → tap variant, both → tap variant, or both glyphs if the spec allows); tests.
- **Bug:** the sleep plan must use the CURRENT settings (after leaving Settings), not the boot-time copy —
  user saw touch wake and the touch indicator in Button mode. Find and fix; test.
- **Bug:** a deep-sleep wake with cause EXT1 and an empty ext1 mask (seen on the device) must classify as
  Button (the rocker is the only EXT1 source); test.
- Settings: the wake mode row cycles 3 values (pips 3); the warn icon next to the value is removed (note text
  stays for Touch and Touch & Button).

## Files to touch
- lib/settings, lib/power, lib/app_logic, lib/settings_ui, src/render/settings_screen.cpp, src/sleep.cpp,
  src/main.cpp (only for the settings-refresh bug), tests

## Acceptance criteria
- [ ] Native tests for migration, plan per mode, the current-settings bug, EXT1 mask 0 → Button.
- [ ] `pio test -e native` and the builds pass.
- [ ] On device: each mode wakes only on its sources; the indicator matches.

## Notes
- PC part done (2026-10-29): cause of the current-settings bug: goToSleep used the session-start plan (session.cpp:132/363); now always app_logic::planForSleep from g_settings. Schema v2 with legacy 1 → TouchAndButton. EXT1 always Rocker. Warn icon removed. 522 tests. Check on device: "Touch & Button" F40 width in the landscape row.
- Device session 2026-10-30: user batch 2 all OK (Button / Touch / Touch & Button wake only on their sources).
