# 129: Flaky SD mount must not load the demo

Status: PC part done (device check pending)
Depends on: none
UI spec: none
Needs device: yes

## Scope
A single failed SD mount or stat deletes the config cache and loads the demo dashboard (fake values for hours on a scheduled wake). Retry the mount (e.g. 3x, short delay); on 'no card' or stat failure keep the cached config; only an explicit, readable change replaces it; demo only when there was never a config. Decision in lib/ with native tests.
Evidence: docs/reviews/2026-11-26-project-review.md (finding H2).

## Files to touch
- src/app_config.cpp, src/config_sd.cpp, lib/config (decision logic), test/

## Acceptance criteria
- [ ] Native tests: mount fail / stat fail / no card with a cache keeps the cache; no cache + no card = demo; changed file = re-import.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Device-only behaviour tagged UNVERIFIED with a docs/device-checks.md line.

## Notes
