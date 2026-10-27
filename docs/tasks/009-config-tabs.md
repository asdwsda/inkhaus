# 009: Config schema: tabs, widget size, group names

Status: done
Depends on: 003
UI spec: none
Needs device: no

## Scope
Adjust config schema v1 in place (nothing is released yet):
- Rename `pages` to `tabs` everywhere (YAML key, model, error paths, constants,
  `config.example.yaml`). Max 16 tabs; widget limit per tab stays 24.
- Optional widget `size: WxH` (e.g. `2x1`), W and H in 1..4. Stored in the model as
  optional; absent means "type default" (decided by the layout engine, not here).
- Optional tab `icon:` is NOT added (later).
- Group names must match `[a-z0-9_]+` (fixes the dotted-path ambiguity noted in task 003).
- Extend `config.example.yaml` to show every supported widget type at least once,
  matching the user's use cases: a single shutter and a shutter group, a light, a switch,
  an automation, a thermostat, an outside temperature sensor and a vacuum, spread over
  at least two tabs (e.g. "Living room" and "Whole house").

## Files to touch
- lib/config/src/*
- config.example.yaml
- test/test_config/*

## Acceptance criteria
- [ ] `tabs` parses; `pages` is now an unknown key, and a config with only `pages` gives the
      "missing tabs" error (tests).
- [ ] `size` parses for valid values; `0x1`, `5x1`, `2x`, `abc` give errors with the right path (tests).
- [ ] A group name with a dot or uppercase gives an error at `groups.<name>` (test).
- [ ] Updated `config.example.yaml` parses clean and covers all widget types (test).
- [ ] All existing tests are updated for the rename, none deleted; `pio test -e native`
      and `pio run -e m5paper` pass.

## Notes
- Result: `size` is strict `[1-4]x[1-4]` (lowercase x, no whitespace). Group names
  `[a-z0-9_]+`. The example config covers all 9 widget types over 2 tabs.
- Accepted: the example test checks "every type appears" instead of per-widget types;
  the type override path is covered by `test_error_unsupported_domain`.
- Cosmetic: an empty group name reports path `groups.` (trailing dot).
