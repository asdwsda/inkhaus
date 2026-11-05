# 124: Refactors after 1.4.0

Status: todo
Depends on: none
UI spec: none
Needs device: no (behaviour must not change)

## Scope
User 2026-11-05: the project has grown; refactor where the code gets clearly better. Step 1: a code-health survey
(tester): largest files/functions (main.cpp first), duplicated logic (e.g. the notifications popup's own confirm dialog
vs the shared one from 108), glue patterns repeated per widget (forecast/calendar/art glue), dead code, release-build
logging (logMem tick). Step 2: the architect picks items; one sub-task per item (124a, 124b, ...), each behaviour-neutral
with the existing tests green.
Known item: notifications popup uses the shared ConfirmDialog.

## Acceptance criteria
- [ ] Survey report in docs/tasks/124-refactor-survey.md Notes (ranked, with file:line and estimated effort).
- [ ] Each picked refactor: `pio test -e native`, `pio run` (all envs) pass; binary size and RAM not worse.

## Notes
- Survey (tester, 2026-11-05): docs/tasks/assets/124-refactor-survey.md.
- Picked (architect), in this order, each behaviour-neutral, one commit each:
  - 124a: one glue helper for calendar/forecast/art (store, pump, result, demo, newSession); fix calendar not
    handling `Cancelled` (forecast uses `unmarkAsked`). Add a native test for the shared part.
  - 124b: notifications use the shared ConfirmDialog + drawConfirmDialog + one timeout constant (after 123 merges).
  - 124c: logging: gate the periodic noise (`logMem("tick")`, `loop_stack_free` per loop) behind DEBUG_SERIAL;
    keep the event lines (APP/PRESENT/BOOT/HA) in release, the device checks rely on them.
  - 124d: split lib/config/src/config.cpp into parse and validate files (no logic change).
  - 124e: main.cpp: move loop() phases (input, net events, pumps, frame, sleep) and the settings glue into
    src/*.cpp files; last, after the others, with a device smoke test.
- Not picked: other long functions (low value per risk), render widget de-duplication (pixel risk), test file splits.
