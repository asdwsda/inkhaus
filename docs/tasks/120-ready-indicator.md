# 120: Show when the device is ready to send commands

Status: todo
Depends on: 116 (net lanes)
UI spec: docs/ui/ready-indicator.md (+ png/ready-indicator.png)
Needs device: yes (look + timing; PC part first)

## Scope
User 2026-11-05: after a wake, show clearly whether a tap will be sent now or has to wait for Wi-Fi/HA. A small status-bar
state (e.g. connecting -> ready -> offline) and/or a tap feedback that says "queued, connecting". Must cost at most
one small partial refresh per state change, no extra full refresh, nothing on scheduled (non-interactive) wakes.
Out of scope: BLE (postponed), changing how the connection itself works.

## Files to touch
- lib/ui_state (state model), src/render (status bar), test/

## Acceptance criteria
- [ ] Native tests: state transitions (wake, Wi-Fi up, HA reachable, lost) and the refresh budget.
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] Device check: wake by rocker, tap at once; the indicator matches when the command actually goes out (log times).

## Notes
- Decision (architect, 2026-11-05): status-bar indicator partials count toward ghost debt by area only, not as one of the 12 feedback refreshes. Reason: otherwise each interactive wake uses 3 of 12.
- PC part done (firmware-dev, 2026-11-05): lib/app_logic ready_state (tracker, 400 ms coalescing, countsByAreaOnly), statusLabel NotReady/Ready/queued, render Link::NotReady/Ready + hourglass, glue in src/app_frame.cpp, present::setIndicatorRegions (indicator partials skip partial/burst count, area counts). Tests: test/test_ready_state. Left: device check (look, timing, wake rect, Ready = Wi-Fi-up approximation).
