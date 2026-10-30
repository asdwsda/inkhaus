# 040: `unknown` is not `unavailable` (covers without position feedback)

Status: done
Depends on: 028, 036
UI spec: docs/ui/widgets.md §11 (update: unknown rows)
Needs device: yes (user's shutters, later)

## Scope (ARCHITECTURE.md 2026-10-29)
User's `cover.livingroom_shutter` from HA: `"state": "unknown"`, attributes `is_closed: null`,
`device_class: shutter`, `supported_features: 11` (OPEN|CLOSE|STOP, no SET_POSITION). The widgets were greyed
out (unavailable look) and unusable.
- lib/ha_client/lib/model: `unknown` must not set `available=false`; only `unavailable` (and a missing entity)
  does. Parse `supported_features` for covers (fetch it) to know whether a position exists.
- lib/ui_state: `unknown` → normal look, value "–" (all domains; binary_sensor "–"; sensor "–"); buttons enabled.
  Cover without position: state text from the optimistic action ("Opening"/"Closing"/"Stopped") until the
  next fetch, else "–".
- lib/app_logic tap intent / lib/session confirmation: for a cover whose state is `unknown` (or without
  position support), the expected state is empty → a successful call confirms; no "Failed" because the state
  didn't change.
- docs/ui/widgets.md §11.3/11.4: update the rows for `unknown` (minimal edit).

## Files to touch
- lib/ha_client (template attrs for cover + parser), lib/model, lib/ui_state, lib/app_logic, lib/session if
  needed, tests, docs/ui/widgets.md (text only)

## Acceptance criteria
- [ ] Native tests with the user's exact JSON (state unknown, supported_features 11): widget normal, "–",
      tap → "Opening", confirmed on a 200 call response with the state still unknown.
- [ ] `unavailable` still gives the unavailable look; `pio test -e native` and the builds pass.

## Notes
- PC part done (2026-10-29): available=false only for unavailable/missing; supported_features fetched for covers; unknown → normal look "-"; optimistic Opening/Closing/Stopped briefly, back to "-" once the call is confirmed (user: no last-command display wanted). 525 tests. Device check pending.
- Device session 2026-10-30: user batch 1 all OK (wake shows cached values before the fetch; cover tap shows Opening).
