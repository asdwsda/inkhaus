# 037: Persist last known states across sleep; never blank values

Status: in-progress
Depends on: 023, 025
UI spec: status bar outdated icon (task 038 spec)
Needs device: yes (later)

## Scope (ARCHITECTURE.md 2026-10-29)
- Serialise the StateStore (entity id, state, available, last_changed, typed + generic attributes) compactly
  (lib/model, with a CRC; tests incl. truncation/corruption), store it in RTC slow memory at sleep (fast,
  deep sleep) and in LittleFS (`/states.bin`, written only when it changed, survives power loss).
- On boot: restore from RTC, else LittleFS, before the first render, so widgets show the last values
  immediately; the record's epoch drives the "outdated" state.
- Never blank values while connecting/offline/stale: widgets keep the last value; only the status bar
  shows the state (task 038).
- Config change (different entity set) → keep matching entities, drop others.

## Files to touch
- lib/model (serialise), new src/state_cache.{h,cpp}, src/main.cpp (restore at boot, save at sleep: a few
  lines), src/sleep.cpp (call), tests

## Acceptance criteria
- [ ] Round-trip and corruption tests; restore prefers RTC, falls back to LittleFS, ignores bad data.
- [ ] `pio test -e native` and the builds pass.
- [ ] On device: wake shows the previous values immediately, before Wi-Fi.

## Notes
