# 045: Brightness popup for dimmable lights

Status: todo
Depends on: 017, 036
UI spec: docs/ui/brightness.md (mockups docs/ui/png/brightness-*.png)
Needs device: yes (later)

## Scope (user request, 2026-10-30)
Dimmable lights (HA `supported_color_modes` includes a brightness mode, or `brightness` attribute present)
get a brightness popup:
- Open: long press on the light widget (touch), or rocker long-push on a focused light. Short tap stays
  on/off (fast path unchanged).
- Content: name, current brightness %, a horizontal bar you tap at a position to set (no dragging: e-ink),
  −10 % / +10 % buttons, presets 25 / 50 / 75 / 100 %, Off, and Done/×. Fast partial refreshes only.
- Behaviour: optimistic value immediately; changes debounced (send the last value after ~400 ms of no
  input) as `light.turn_on` with `brightness_pct`; `brightness_pct: 0` → `light.turn_off`. Close on Done,
  tap outside, or 10 s inactivity. Rocker: up = +10 % (brighter), down = −10 %, push = Done, long push = close.
- Widget shows brightness when on ("On 60%" already exists).

## Steps
1. ui-designer: spec + mockups (portrait/landscape, popup over the tab), check_mockups.
2. firmware-dev: HA (fetch `supported_color_modes`, brightness_pct call), lib/ui_state (popup model,
   debounce), render, input (long press), tests.

## Acceptance criteria
- [ ] Spec approved by the user; native tests for the model, debounce and service calls; builds pass.
- [ ] On device: long press opens, steps and presets change the real light quickly.

## Notes
