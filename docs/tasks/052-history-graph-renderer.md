# 052: History graph renderer

Status: done
Depends on: 048, 050
UI spec: docs/ui/history.md
Needs device: no (screenshot later)

## Scope
Per 046 task split item 5: draw line+band, step lane and bar into the canvas from lib/history output, with axes and labels per the spec. Pure layout maths in lib/ (testable); drawing in src/render.

Research and design: 046-history-research.md (accepted by the user 2026-10-29).

## Files to touch
- lib/history or lib/ui_state (layout), src/render/history_*.{h,cpp}, tests

## Acceptance criteria
- [ ] Native tests for the layout maths (axis ticks, column mapping, label fit).
- [ ] Builds; device screenshot vs mockup listed in docs/device-checks.md.

## Notes
- Includes time-axis ticks (not in 050).
- 2026-10-29: PC done. Follow-ups: time ticks should not jump between 3 h and 6 h with the window position (pick the step from the range only); unavailable spans need a Series change to draw the MID marker.
