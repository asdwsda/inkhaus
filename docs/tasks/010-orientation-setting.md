# 010: Orientation setting

Status: done
Depends on: 002
UI spec: none
Needs device: no

## Scope
Add `orientation` to the settings module: enum `Portrait` | `Landscape`, default
`Portrait`, NVS key `orientation`, allowed values in display order
(`Portrait`, `Landscape`). Same load/save/validation rules as the other fields.
Schema version stays 1 (the new key is simply missing on old stores → default).

## Files to touch
- lib/settings/src/*
- test/test_settings/*

## Acceptance criteria
- [ ] Default is `Portrait` on an empty store and on a store written before this field existed (tests).
- [ ] Invalid stored value falls back to default for this field only (test).
- [ ] Round-trip and write-only-changed tests include the new field (existing tests extended, none deleted).
- [ ] `pio test -e native` and `pio run -e m5paper` pass.

## Notes
