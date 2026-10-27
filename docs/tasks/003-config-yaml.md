# 003: config.yaml schema and parser

Status: todo
Depends on: 001
UI spec: none
Needs device: no

## Scope
Parse and validate the SD card config into a typed model.

Schema v1 (write it as `config.example.yaml` in the repo root, with comments and fake values):
```yaml
version: 1
connection:                      # optional; if missing, firmware falls back to secrets.h
  wifi:
    ssid: "MyWifi"
    password: "secret"
    static_ip:                   # optional, all four fields required if present
      ip: 192.168.1.50
      gateway: 192.168.1.1
      subnet: 255.255.255.0
      dns: 192.168.1.1
  home_assistant:
    internal_url: http://192.168.1.10:8123
    external_url: https://example.ui.nabu.casa   # optional, must be https
    token: "long-lived-token"
groups:                          # optional, device-side entity lists
  downstairs_shutters: [cover.living_room, cover.kitchen]
pages:                           # at least one
  - title: Living room
    widgets:
      - entity: light.living_room
        name: Ceiling            # optional, default: HA friendly_name
      - group: downstairs_shutters
        name: Downstairs shutters  # required for groups
      - entity: climate.living_room
      - entity: sensor.outside_temperature
        type: sensor             # optional override; normally inferred from domain
```
- Widget type is inferred from the entity domain: `cover`, `light`, `switch`,
  `automation`, `climate`, `sensor`, `binary_sensor`, `weather`, `vacuum`.
  Other domains are a validation error unless `type` is given.
- A group widget: all members must share one domain; type comes from that domain.
- Exactly one of `entity` / `group` per widget.

Implementation:
- `lib/config`: libyaml → ArduinoJson document → typed `Config` model
  (connection optional, pages, widgets, groups, plus the de-duplicated list of
  all entity ids referenced, which the HA client will fetch).
- Validation returns a list of errors, each with a path (`pages[0].widgets[2].entity`)
  and, where libyaml provides it, the line number. Parsing never crashes on bad input.
- Limits (constants): max 16 pages, max 24 widgets per page, max 16 entities per group,
  max 64 KB file size. Over a limit gives a validation error.
- `src/config_sd.{h,cpp}`: read `/config.yaml` from the SD card into a buffer and call
  the parser. Build-only in this task. No LittleFS cache yet.
- Add `/config.yaml` handling note to `config.example.yaml` header comment (copy to SD root).

Out of scope: caching the parsed model, remote editing, UI for errors.

## Files to touch
- lib/config/src/*
- src/config_sd.h, src/config_sd.cpp
- config.example.yaml
- test/test_config/*

## Acceptance criteria
- [ ] `config.example.yaml` parses with zero errors into the expected model (test).
- [ ] Config without `connection:` parses; `connection` is reported absent (test).
- [ ] Each of these gives a specific error with the right path (one test each):
      missing `pages`, widget with both `entity` and `group`, unknown group,
      group with mixed domains, unsupported domain without `type`,
      `external_url` not https, partial `static_ip`, wrong `version`.
- [ ] Malformed YAML (bad indentation, tabs, truncated file) returns an error with a line number, no crash (tests).
- [ ] Limits are enforced (tests).
- [ ] The referenced-entity list is de-duplicated and includes group members (test).
- [ ] `lib/config` has no Arduino includes; `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Never read or create a real `config.yaml`; it's gitignored and holds credentials.
