# 138: Review leftovers L5, L7, L8 (DNS once, net task wakes on demand, dead debug code)

Status: todo
Depends on: none
UI spec: none
Needs device: no (device check later)

## Scope
From docs/reviews/2026-11-26-project-review.md (LOW section), items L5, L7 and L8 only:
- L5: resolve DNS once per new connection (use the `WiFi.hostByName` result, or drop it).
- L7: the net task blocks on its queue (or a task notification) instead of polling every 20 ms.
- L8: remove the dead `boot::log("version", "017")` calls and the `BOOT: dbg_*` trace lines, and any other
  dead or misleading code named in L8.
Out of scope: other review items. Keep the 130 no-double-send rules and the 136 timeouts unchanged.

## Files to touch
- src/ha_transport_arduino.cpp, src/net_task.cpp, src/main.cpp, src/boot.cpp (as the review names)

## Acceptance criteria
- [ ] One DNS lookup per new connection (log `NETT: dns=` counts it once).
- [ ] The net task has no fixed-interval poll; user calls are still picked up at once (latency unchanged).
- [ ] No `dbg_` boot lines and no `version=017` line remain.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
