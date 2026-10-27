# 011: Layout engine update for compact tabs

Status: todo
Depends on: 006, revised docs/ui/layout.md
UI spec: docs/ui/STYLE.md, docs/ui/layout.md, docs/ui/widgets.md
Needs device: no

## Scope
Bring `lib/ui` in line with the revised spec (compact tab bar, possibly merged with the
status bar, new grid geometry, Settings entry via long push, refresh via status-bar tap).
- Update geometry, tab groups/overflow, hit-test kinds (add `RefreshLabel` or as specified),
  and rocker long-push behaviour (TAB level → open Settings).
- Climate: mode row is display-only; remove `climateModeEnabled` paths if the spec drops them.
- Status bar region for the sleep indicator (rect from the spec), so it can be redrawn alone.
- Fix `lib/ui/src/ui_rocker.cpp:107` negative modulo for out-of-range `tab` and un-ignore
  `test_robust_focus_negative_tab_known_bug`.

## Files to touch
- lib/ui/src/*
- test/test_ui_layout/*

## Acceptance criteria
- [ ] Every test vector in the revised layout.md section 8 is a passing test (old vectors updated, not deleted silently: each changed vector is mentioned in the report).
- [ ] Rects match the revised SVG mockups (mockup tests updated).
- [ ] Hit-test returns the refresh target for the "updated" label; long push in TAB mode yields an `OpenSettings` effect (tests).
- [ ] `test_robust_focus_negative_tab_known_bug` passes (no longer ignored).
- [ ] Property, robustness and sanitizer runs from task 006 still pass; `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Placeholder until the revised spec lands; the architect finalizes scope after review.
