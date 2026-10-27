# 006: UI layout engine

Status: todo
Depends on: 009, 010
UI spec: docs/ui/STYLE.md, docs/ui/layout.md
Needs device: no

## Scope
Pure layout logic in `lib/ui`, following docs/ui/layout.md exactly (grid sizes,
margins, tab bar, sub-page indicator). No drawing.
- Input: orientation (from settings), the tab list from the config model
  (titles + widgets with type and optional size), the active tab, the active sub-page.
- Output: a `Layout` with pixel rectangles for the tab bar, each tab button, the
  sub-page indicator and each visible widget (plus the widget's cell size).
- Grid per orientation as in the spec. Default size per widget type from the spec;
  config `size` overrides it; a size that doesn't fit the grid is clamped and reported.
- Flow: row-major, first fit; when a tab overflows, continue on a new sub-page.
- Hit-testing: point → tab button / widget index (+ the widget's sub-region id for
  multi-button widgets, as defined in the spec) / nothing.
- Rocker focus model as described in the spec: next/previous focus order, wrap rules,
  push = activate.
- Rotation-aware rocker mapping (which physical direction is "up" in each orientation).

Out of scope: drawing, widget internals beyond sub-region rectangles, settings screen.

## Files to touch
- lib/ui/src/*
- test/test_ui_layout/*

## Acceptance criteria
- [ ] Grid dimensions and tab bar rectangles match docs/ui/layout.md for both orientations (tests).
- [ ] Default sizes per widget type match the spec; config size override works; oversize is clamped (tests).
- [ ] Flow fills gaps first-fit and overflows into sub-pages; sub-page count is correct (tests).
- [ ] No two widget rectangles overlap and all are inside the content area, for random
      widget lists in both orientations (property test, fixed seed).
- [ ] Hit-testing returns the right tab / widget / sub-region at rectangle edges and gaps (tests).
- [ ] Rocker focus order and wrap behave as specified, in both orientations (tests).
- [ ] `lib/ui` has no Arduino includes; `pio test -e native` and `pio run -e m5paper` pass.

## Notes
