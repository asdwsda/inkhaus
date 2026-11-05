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
