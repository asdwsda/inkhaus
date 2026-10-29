# 049: History config keys

Status: todo
Depends on: 046
UI spec: none
Needs device: no

## Scope
Parse the per-widget `history:` block (hours, type, statistic, period, min, max, source), the shorthand `history: <hours>` and `history: false`, per 046 section 4. Validation errors with the YAML path, never crash. Docs/example config.

Research and design: 046-history-research.md (accepted by the user 2026-10-29).

## Files to touch
- lib/config (Widget::history), test/test_config, config.example.yaml, README config section

## Acceptance criteria
- [ ] Native tests: defaults, block, shorthand, false, every invalid value (hours range, min>=max, bad enums, unknown key).
- [ ] `pio test -e native` and `pio run -e m5paper` pass.

## Notes
