# 113: Build info in Settings

Status: done (user saw it on the device, 2026-11-04)
Depends on: 087
UI spec: docs/ui/settings.md (Tools page; ui-designer adds the row)
Needs device: yes (look)

## Scope
Show which firmware is running: version, build variant (release / debug / batterytest / epdoff / poweroff ...), short git
commit and build date, e.g. `1.3.1 debug · 7727f0d · 2026-11-03`, on Settings > Tools (About row) and in the boot log line.
Variant comes from a build flag per env in platformio.ini; commit/date from a PlatformIO extra script (dirty tree -> `+`).
Reason (user 2026-11-03): long-running test builds were mistaken for normal ones.

## Acceptance criteria
- [ ] Every env sets its variant; native test for the formatting/fit rule (ellipsis on narrow widths).
- [ ] `pio run` all envs; boot log `BOOT: fw=<text>`; device look check.
