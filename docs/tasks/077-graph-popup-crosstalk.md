# 077: Long press on another sensor breaks the graph widget

Status: done
Depends on: 071, 074
UI spec: docs/ui/history-widget.md, docs/ui/history.md
Needs device: yes

## Scope
User report (2026-10-30): on the tab with the "Outside" graph widget, long-pressing the `sun.sun` widget
(`type: sensor`, 2x1, on the same tab) makes the Outside graph widget show "loading" and then
"unavailable", as if the popup's history job were drawn into the graph widget. Find the root cause
(suspects: the shared `planHistoryJob` / job slot or result routing between the popup and graph widgets
in lib/ui_state, the per-entity history cache key, the merge regions of 074, or popup close/redraw) and
fix it.

## Files to touch
- lib/ui_state (graph_widgets, history_popup, app_state), src/main.cpp, tests as needed.

## Acceptance criteria
- [ ] Native test that reproduces it first: a popup history job for entity A while graph widget B is
  shown leaves B's data and state unchanged, and the popup gets A's result.
- [ ] The popup for a non-numeric sensor (`sun.sun`) shows its normal state or "no data" look per the
  spec.
- [ ] Device: long press Sun, close the popup; the Outside graph keeps its graph (user check).
- [ ] `pio test -e native`, `pio run -e m5paper` and `-e m5paper-debug` pass.

## Notes
- Root cause (2026-10-30): the 074 `mergeIntoRegions` cut a diff rect that overlapped a graph widget
  down to the widget rect, so the popup (y 212-730) was refreshed only inside the graph area. There
  was no data crosstalk. Fix: a rect extending beyond the region is kept and united with it
  (frame_diff.cpp); test `test_merge_into_regions_keeps_rect_extending_beyond_region`. Release
  build flashed.
- Done 2026-10-30: user check on the device ok.
