# 041: Debug-log setting, Advanced settings page, cold-boot splash

Status: done
Depends on: 029, 034, 038
UI spec: docs/ui/settings.md (Advanced page), docs/ui/splash.md (new) — ui-designer
Needs device: yes (later)

## Scope
- **Spec (ui-designer):** an "Advanced" Settings page reached from the main Settings page (e.g. a footer button
  or a last row), holding Demo mode (moved from the main page, which goes back to 4 rows) and Debug log
  (Off/On). Both orientations, rocker focus, Back to the main page. A cold-boot **splash screen**: app name,
  a simple e-ink-friendly logo/illustration, version, both orientations; shown once on a cold boot, then the
  normal UI.
- **Firmware, settings + logging:** `debug_log` setting (NVS key `dbglog`, default off). The SD wake log
  (task 029) is compiled into every build and runs only when the setting is on (no SD access otherwise).
  When on, also append the key log lines (APP:/NET:/SLEEP:/WAKE:/BOOT: start/config/errors; never secrets)
  to `/debug.log`, capped at 1 MB (rotate to `/debug.old`). The `m5paper-batterytest` env keeps forcing it on.
  `wakelog`/a new `debuglog` serial command available in normal builds (read-only).
- **Firmware, UI:** the Advanced page and the splash per spec (after the spec).
- **Cold boot** = reset reason POWERON/EXT and no valid screen record (or the record's firmware differs).

## Acceptance criteria
- [ ] Spec + PNGs approved by the user.
- [ ] Native tests: setting field + migration, the log gate, the log line filter (no secrets), rotation.
- [ ] `pio test -e native` and the builds pass.
- [ ] On device: toggling Debug log starts/stops `/wakelog.csv` + `/debug.log`; splash on a cold boot only.

## Notes
- Logic part done (2026-10-29): debug_log setting (NVS dbglog), lib/app_logic debuglog filter (no secrets) + rotation, wakelog compiled in every build and gated at runtime, /debug.log buffered and appended at the existing SD mounts, debuglog/wakelog serial commands, tools/wakelog.py --file. Open for the UI step: Advanced page row; route net.cpp/boot.cpp key lines through wakelog::logf.
- UI part done (2026-10-29, PC only, not flashed): settings_ui main page (4 rows + Tools row) and Tools page, render of both, splash (logo via gen_icons), net/boot lines through wakelog::logf. Next: device check (splash on cold boot only, Tools page, debug log toggle).
