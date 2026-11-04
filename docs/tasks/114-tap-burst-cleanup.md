# 114: Clean the screen after a burst of taps (factory-app style)

Status: done
Depends on: 092, 095, 099
UI spec: none
Needs device: yes (look + timing)

## Scope
Grey traces build up from fast partial refreshes during taps (user 2026-11-03: "ugly after a few taps"). Do what the
M5Paper factory app does (docs/tasks/assets/099-epd-research.md): once more than N fast partials (default 4) happened since
the last full and the user has been idle for T (default 2 s), do one full refresh of the whole frame. Experiment variants on
m5paper-debug via serial (`burstclean N T mode`, mode = gc16 | gl16 | init+gl16 using task 099's INIT helper), defaults
picked after the device look. Never during popups/Settings/OTA, never while a tap is pending; resets the partial debt (092)
and the 095 idle timer. Log `PRESENT: refresh=full_burst partials=N mode=..`.

## Acceptance criteria
- [ ] Pure decision logic in lib/ with native tests (counter, idle timer, exclusions, reset by any full).
- [ ] `pio test -e native`, `pio run` all envs; device: 10 taps then idle 2 s -> one full, screen clean (user look).
