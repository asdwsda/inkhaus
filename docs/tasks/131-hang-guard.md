# 131: Hang guard after setup and on fatal errors

Status: done (PC part; device check pending, see docs/device-checks.md)
Depends on: none
UI spec: none
Needs device: yes

## Scope
After setupDone() no watchdog covers loop(); crash loops inside loop() aren't counted; 'Out of memory' never sleeps (Wi-Fi on); showError doesn't invalidate the screen record. Add a task watchdog on loop() (fed per pass, generous timeout), count loop crashes like boot crashes, sleep after a fatal error screen, invalidate the record in showError.
Evidence: docs/reviews/2026-11-26-project-review.md (finding M2).

## Files to touch
- src/boot.cpp, src/main.cpp, src/present.cpp, lib/app_logic (crash-loop counter), test/

## Acceptance criteria
- [ ] Native tests for the crash-loop counter/policy.
- [ ] Fatal error path ends in sleep within a bounded time.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Device-only behaviour tagged UNVERIFIED with a docs/device-checks.md line.

## Notes
