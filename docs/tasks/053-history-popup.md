# 053: History popup (input, cache, refresh)

Status: PC done; device check pending (rocker focus decision open)
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
- 2026-10-30 tester review (643/643 native, build OK). Fixes required (architect decisions):
  F1 Popup blocks idle sleep (src/main.cpp ~778): an open history popup counts as activity only while a fetch is
     running; after that the normal idle timer runs and goToSleep closes it via historyShutdown (spec: "closes
     before the device sleeps"). Reason: no extra timeout constant, matches the spec.
  F2 Two modals at once: guard dimmableLightAt/openBrightness/longPress (app_state.cpp ~368-376) with
     hist_.isOpen() (any modal open => ignore); a LongTouch while history is open does nothing. Add tests.
  F3 Timeout with 0 bytes maps to Unreachable (keep stale data, "cached, offline"), not TooMany (main.cpp ~290).
     Timeout after data stays TooMany. A failure must not drop a good cached graph.
  F4 sameColumns compares with a tolerance of the cache quantisation step ((hi-lo)/65534), not exact floats.
  F5 (device check) measure SD lookup cost on reopen; F6 FNV collision is safe, no change.
  Fixed F1-F4 (646/646 native, build OK). Left as is: a hold that starts outside the popup closes it, then the LongTouch may open brightness under the finger (minor).
