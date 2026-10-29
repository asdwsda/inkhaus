# 052: History graph renderer

Status: todo
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
