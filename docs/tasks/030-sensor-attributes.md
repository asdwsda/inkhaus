# 030: Configurable sensor attributes (config + data)

Status: in-progress
Depends on: 003, 004, 017
UI spec: none for this part (rendering is task 031)
Needs device: no

## Scope
User request: choose which attribute(s) of a sensor to show. Example: a climate sensor entity with
attributes temperature, humidity, dew_point.
- **Config** (lib/config, schema v1, optional keys on any widget, validated with path + line, and
  error messages that never echo values):
  - `attribute: <name>`: show this attribute as the main value instead of the state;
  - `details: [<attr>, <attr>]`: up to 2 extra attributes for a details line (more → error);
  - `unit: "<text>"`: unit override for the main value (≤ 8 chars);
  - `decimals: <0..3>`: round numeric main/detail values.
  Attribute names: `[a-z0-9_]+`, ≤ 40 chars. Update config.example.yaml with the example above
  (commented, so the demo stays unchanged) and the config cache serialisation (task 025) incl. tests.
- **Fetch** (lib/ha_client): the template builder fetches, per entity, the union of its domain's
  fixed attributes and the attributes requested by any widget for that entity, each value via
  `tojson`. Keep the injection safety (names are validated).
- **Model** (lib/model): `EntityState` gets a small generic map attribute → raw JSON scalar as
  text (string, number, bool; nested objects/arrays → skipped); range-check numbers as today.
- **View data** (lib/ui_state): `sensorView` takes the widget's options: main value from the
  attribute if set (with `humanise` for text, `decimals`, `unit` override, else the attribute's own
  unit if HA provides one, else the entity's `unit_of_measurement` only when showing the state),
  plus up to 2 detail items {label, value, unit}. Labels: humanised attribute name
  (`dew_point` → "Dew point"). Missing attributes → "-".

## Files to touch
- lib/config, lib/ha_client, lib/model, lib/ui_state + their tests; config.example.yaml

## Acceptance criteria
- [ ] Config parsing/validation tests for every key incl. errors; cache round-trip covers the new fields.
- [ ] Template golden test with a requested attribute; parser test for scalar/nested/missing attributes.
- [ ] sensorView tests: attribute main value, decimals, unit rules, two details, missing → "-".
- [ ] `pio test -e native` and the builds pass.

## Notes
