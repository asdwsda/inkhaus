# 125: The tap that wakes the device is consumed

Status: todo
Depends on: 120
UI spec: docs/ui/ready-indicator.md (Interactions: "the waking tap is consumed")
Needs device: yes (touch wake check; PC part first)

## Scope
A touch that wakes the device from sleep must not act on what is under the finger (no toggle, no
popup, no tab switch, no history). It only wakes; the ready indicator draws `not ready, connecting`
as today. Rocker wake is unchanged. First check what the code does now (the touch wake path in
src/app_sleep.cpp / app_input.cpp / boot); if the tap is already dropped, add the missing test and a
note, nothing else.
Out of scope: changing the rocker wake, the indicator states, or taps after the wake.

## Files to touch
- the touch-wake / input path in src/ (app_input.cpp, app_sleep.cpp or similar)
- lib/ if the rule fits a pure function there (preferred, for a native test), test/

## Acceptance criteria
- [ ] The waking touch (down and its release) produces no widget, tab, popup or settings action.
- [ ] A new touch after the waking one has been released works normally (no extra delay added).
- [ ] The rule is covered by a native test, or, if it lives only in src/, the reason is in Notes.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] docs/device-checks.md line; UNVERIFIED tag on code not checked on the device.

## Notes
