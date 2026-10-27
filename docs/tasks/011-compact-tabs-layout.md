# 011: Layout engine update to the approved compact UI

Status: done
Depends on: 006
UI spec: docs/ui/STYLE.md, docs/ui/layout.md, docs/ui/widgets.md (revision 4, commit 3bce7d3)
Needs device: no

## Scope
Bring `lib/ui` in line with the approved spec. Reference images: docs/ui/png/.
- Geometry: 104 px cells with 12 px gaps; portrait 2x7 with a 56 px status bar and a bottom
  tab bar; landscape 5x4 with one combined 64 px top bar (refresh label, 5 tab slots, battery).
  Status bar, refreshHit, settingsHit, sleep-indicator, tab bar, slot, content and pager rects
  exactly as layout.md.
- Per-orientation size table per widget type (layout.md section 3): every widget is 1 row;
  toggles 1x1; cover/climate/vacuum wide (2 cells portrait, 3 landscape); clamp reports.
- `subRegions` for the new widget internals (64x64 buttons at W-228/W-152/W-76, y=20;
  climate minus at W-276, plus at W-76).
- Hit kinds: add `Refresh` (status "updated" label) and `Settings` (battery cluster).
  Remove `climateModeEnabled` everywhere (mode is display-only).
- Rocker: LONG_PUSH at TAB level gives an `OpenSettings` effect; apply the section 7.2
  clarifications (end of list keeps the sub-page, first event after wake acts and shows
  the ring, no further focusable sub-page means end).
- Expose the sleep-indicator rect per orientation so it can be redrawn on its own.
- Fix `lib/ui/src/ui_rocker.cpp:107` negative modulo and un-ignore
  `test_robust_focus_negative_tab_known_bug`.
- `config.example.yaml`: remove `size:` values the new size table no longer allows
  (2x1 on the group, 2x2 on climate and weather), so the example parses with no clamp
  reports; update `test_example_parses_clean` accordingly (assert the absence of size,
  don't drop other assertions).

## Files to touch
- lib/ui/src/*
- test/test_ui_layout/*
- config.example.yaml
- test/test_config/test_main.cpp (only `test_example_parses_clean`)

## Acceptance criteria
- [ ] Every test vector in layout.md section 8 (revision 4) is a passing test; the report
      lists which old tests changed and why.
- [ ] Mockup tests: computeLayout on the mockup widget list gives exactly the `w-N-frame`
      rects of docs/ui/tab-portrait.svg and docs/ui/tab-landscape.svg (read the numbers
      from the SVGs), plus the status/tab/slot rects.
- [ ] Hit tests return `Refresh` and `Settings` at their rects' edges; LONG_PUSH in TAB mode
      yields `OpenSettings` (tests).
- [ ] `test_robust_focus_negative_tab_known_bug` passes and is no longer ignored.
- [ ] `config.example.yaml` parses with zero errors and zero clamp reports in both orientations (test).
- [ ] The property, robustness and sanitizer tests from task 006 still pass; no Arduino in lib/ui;
      `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Result: geometry, size table, sub-regions, `Refresh`/`Settings` hit kinds,
  `OpenSettings` on TAB long push, `sleepIndicatorRect()`, and the negative-modulo fix. 70 layout
  tests, including a full-screen brute-force hit oracle; the old→new coverage map in the tester
  report shows no lost coverage. Compiles with 0 warnings on xtensa (scratch build).
- Accepted readings: LONG_PUSH in TAB mode leaves the ring state unchanged; `subRegions` keeps
  an unused `size` parameter.
- For the next spec touch-up (ui-designer): layout.md 7.2 ring wording vs LONG_PUSH,
  section 6 `size` parameter, 8.4 pager hit vectors, STYLE.md section 4 height derivation wording.
