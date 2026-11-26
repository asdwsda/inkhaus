# 133: Run all tests on every push

Status: done
Depends on: none
UI spec: none
Needs device: no

## Scope
Native tests run only on release tags; site JS tests and tools/tests/test_provision.py never run. Add a push/PR workflow: pio test -e native, pio run (all envs), site node tests, python tool tests. Cache PlatformIO.
Evidence: docs/reviews/2026-11-26-project-review.md (finding M4).

## Files to touch
- .github/workflows/

## Acceptance criteria
- [ ] Workflow runs on push and PR; all jobs green on main (check with gh run list after the push).
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Device-only behaviour tagged UNVERIFIED with a docs/device-checks.md line.

## Notes
