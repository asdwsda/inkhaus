# 019: Explicit on/off actions, actions before the first fetch

Status: in-progress
Depends on: 004, 018
UI spec: none
Needs device: no

## Scope
Apply the 2026-10-28 decision (ARCHITECTURE.md):
- `lib/ha_client`: add `TurnOn`/`TurnOff` actions for light and switch (automation already has
  them); keep `Toggle` for compatibility but stop using it from the UI.
- `lib/ui_state` (fake actions + view) and wherever the UI maps a tap to an action: a tap on a
  toggle widget sends `TurnOn` if the shown state is off, `TurnOff` if it is on.
- `lib/session`: queued actions are sent before the initial full fetch; the confirmation fetch
  follows. Cancellation for explicit actions: tapping twice (on → off) leaves the last explicit
  action = the originally shown state, so nothing is sent.
- Climate `set_temperature` as a first-class action in `ha_client` (replaces the custom
  ServiceCall path in session, if it simplifies things).

## Files to touch
- lib/ha_client/src/*, test/test_ha_client/*
- lib/session/src/*, test/test_session/*
- lib/ui_state/src/*, test/test_ui_state/* (only the tap → action mapping)

## Acceptance criteria
- [ ] Service bodies for light/switch turn_on/turn_off (tests).
- [ ] Session sends queued actions first, then fetches; "on then off" before Wi-Fi sends nothing (tests).
- [ ] The UI maps taps to explicit actions from the shown state (tests).
- [ ] `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Starts after 017 (lib/ui_state) and 018 are merged.
- Session 1 result (2026-10-28, uncommitted, not yet reviewed): Light/Switch TurnOn/TurnOff actions;
  `uistate::toggleIntent` from the shown state; session sends queued actions before the initial
  fetch, opposite explicit actions cancel; the initial fetch also confirms matching sent actions.
  362 native tests pass. Next: tester review, then commit. For the src wiring: map toggle taps via
  toggleIntent; the driver must handle ServiceDone before the first FetchDone.
- Review (2026-10-28): criteria PASS except one bug: `lib/session/src/session.cpp:247`
  `on_service_done` pushes the in-flight item into `confirm_` although the queue holds a newer
  action for the same id; result: a stale "on" confirmation → `Mark::Failed` for an entity in the
  right state (test `test_t019_opposite_tap_while_first_call_in_flight` reproduces it, currently failing).
  Fix: skip/dedupe by id. Also: `toggleIntent` must follow the mixed-group decision (any member on
  → TurnOff; ARCHITECTURE.md 2026-10-28). Do both after task 023 has finished (it may touch lib/session
  and lib/ui_state). Climate first-class action: optional, not done.
