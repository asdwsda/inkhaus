# 076: Smaller demo (4 tabs)

Status: in-progress
Depends on: 033, 068, 071
UI spec: none
Needs device: no (a quick look in demo mode before the release)

## Scope
Replace demo.yaml with the approved 4-tab layout (user, 2026-10-30): docs/tasks/assets/076-demo-proposal.yaml
(tester proposal; fit checked through computeFlow, one page per tab in portrait and landscape).
Optional: remove demo_states.cpp entries that no demo entity uses any more, if that is simple.

## Files to touch
- demo.yaml, test/test_ui_state/test_main.cpp, lib/ui_state/src/demo_states.cpp (optional tidy),
  docs that describe the demo tabs (docs/DEVELOPMENT.md, README if needed).

## Acceptance criteria
- [ ] demo.yaml has 4 tabs as in the proposal; every widget type plus a graph widget, history popup and
  2x1 sensor appear.
- [ ] `test_demo_yaml_showcase` is updated: it asserts 4 tabs and no pager in either orientation, and
  keeps its other checks.
- [ ] `pio test -e native`, `pio run -e m5paper` and `pio run -e m5paper-debug` pass.
