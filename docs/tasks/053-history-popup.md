# 053: History popup (input, cache, refresh)

Status: PC part in progress (see Notes, 2026-10-30 handoff)
Depends on: 049, 050, 051, 052
UI spec: docs/ui/history.md
Needs device: yes
Also includes (from 051): the net task "history fetch" job type.

## Scope
Per 046 task split item 6: long press / rocker long-push on a sensor or binary widget opens the popup (lights keep the brightness popup); popup state machine; PSRAM + SD cache (show stale, then refresh, changed-only redraw); Loading frame fast, graph one quality refresh; close restores the screen.

Research and design: 046-history-research.md (accepted by the user 2026-10-29).

## Files to touch
- lib/ui_state, src/main.cpp, src/render, src/input (if needed), tests

## Acceptance criteria
- [ ] Native tests for the popup state machine and cache freshness.
- [ ] Device: open time logged (`HIST: fetch_ms parse_ms bytes points`), no ghosts after close, cached reopen shows at once.

## Notes
- 2026-10-30 handoff (session paused at the usage limit). Done on PC: 654b478, 323ad1b, Loading hint commit.
  Open: (1) tester review of 053 + wiring was interrupted; rerun it (focus: net-task/UI concurrency, cancel
  and ticket, PSRAM/SD cache, fetch in flight at sleep, one modal at a time, refresh sequence, no URL/token logs).
  (2) User decision pending: make sensor/binary widgets rocker-focusable when history is on (architect
  recommends yes; push/long-push opens the graph). `AppState::openHistoryFocused` is already wired.
  (3) Then mark 052/053 "PC done; device check pending" in README. 054 (WS statistics) not started.
