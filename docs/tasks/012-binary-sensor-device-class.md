# 012: binary_sensor device_class

Status: done
Depends on: 004
UI spec: docs/ui/widgets.md (binary_sensor section)
Needs device: no

## Scope
Fetch `device_class` for `binary_sensor` entities too, so the UI can say "Open/Closed"
for a window instead of "On/Off" and pick the matching icon.
- Add `device_class` to the binary_sensor attribute list in the template builder
  (it is already fetched for `sensor`).
- Parser stores it in `EntityState` (already has the field for sensor).

Out of scope: the wording/icon table itself (belongs to the widget drawing task).

## Files to touch
- lib/ha_client/src/template_builder.cpp
- lib/ha_client/src/state_parser.cpp (only if needed)
- test/test_ha_client/*

## Acceptance criteria
- [ ] Template for a binary_sensor includes `device_class` (golden test updated).
- [ ] Parser fills `device_class` for binary_sensor; missing → absent (tests).
- [ ] `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Raised by ui-designer (revision 4, OPEN-1); the window mockup reads "On" today.
- Result: one line in `domain_attrs`; parser unchanged (already generic). Verified with a
  jinja2 render (with and without the attribute) and the full native suite.
