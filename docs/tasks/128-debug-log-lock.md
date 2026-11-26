# 128: Debug log: lock the shared buffer

Status: todo
Depends on: none
UI spec: none
Needs device: no

## Scope
The net task and the OTA job append to the debug-log buffer (g_pending/g_rec) while the UI loop appends and clears it, unlocked: heap corruption when Debug log is on. Add one mutex (or a lock-free per-core queue) around every access; never hold it across I/O.
Evidence: docs/reviews/2026-11-26-project-review.md (finding H1).

## Files to touch
- src/wakelog.cpp, src/ha_transport_arduino.cpp, src/ota.cpp, src/net_task.cpp

## Acceptance criteria
- [ ] Every g_pending/g_rec access is under the lock (list in Notes).
- [ ] No blocking SD/serial write while holding it.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Device-only behaviour tagged UNVERIFIED with a docs/device-checks.md line.

## Notes
