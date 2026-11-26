# 132: At most one GC16 full per scheduled wake

Status: done (PC part; device check pending)
Depends on: none
UI spec: none
Needs device: yes

## Scope
With an invalid screen record (first wake after an update/crash), the scheduled path runs present.cpp:551 before the deferred check at :566 and stacks 2-3 GC16 fulls. Decide the wake's full once and run it once.
Evidence: docs/reviews/2026-11-26-project-review.md (finding M3).

## Files to touch
- src/present.cpp, lib/screen_track, test/

## Acceptance criteria
- [ ] Native test: invalid record + scheduled + debt = exactly one full.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Device-only behaviour tagged UNVERIFIED with a docs/device-checks.md line.

## Notes
