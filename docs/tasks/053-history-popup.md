# 053: History popup (input, cache, refresh)

Status: todo
Depends on: 049, 050, 051, 052
UI spec: docs/ui/history.md
Needs device: yes

## Scope
Per 046 task split item 6: long press / rocker long-push on a sensor or binary widget opens the popup (lights keep the brightness popup); popup state machine; PSRAM + SD cache (show stale, then refresh, changed-only redraw); Loading frame fast, graph one quality refresh; close restores the screen.

Research and design: 046-history-research.md (accepted by the user 2026-10-29).

## Files to touch
- lib/ui_state, src/main.cpp, src/render, src/input (if needed), tests

## Acceptance criteria
- [ ] Native tests for the popup state machine and cache freshness.
- [ ] Device: open time logged (`HIST: fetch_ms parse_ms bytes points`), no ghosts after close, cached reopen shows at once.

## Notes
