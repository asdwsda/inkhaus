# 130: Service calls are never sent twice

Status: todo
Depends on: none
UI spec: none
Needs device: no

## Scope
A reused socket failing with CONNECTION_LOST is resent, and any status 0 (incl. read timeout after the request was written) falls back to the other URL: non-idempotent calls (next track, volume step, cover/garage, alarm) can run twice. Retry/fallback only when the request provably wasn't sent (connect failure, write failure before any byte); otherwise report failure. Reads (GET/fetch) keep retrying.
Evidence: docs/reviews/2026-11-26-project-review.md (finding M1).

## Files to touch
- src/ha_transport_arduino.cpp, lib/ha_client (keep_alive, ha_client), test/

## Acceptance criteria
- [ ] Native tests: POST after a read timeout / lost after write is not retried nor sent to the other URL; connect failure still falls back; GET keeps retrying.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Device-only behaviour tagged UNVERIFIED with a docs/device-checks.md line.

## Notes
