# Configuration reference

Inkhaus reads two files from the root of the SD card (or uploaded with the installer):
`secrets.yaml` (private) and `config.yaml` (your dashboard). Examples:
[`secrets.example.yaml`](../secrets.example.yaml) and [`config.example.yaml`](../config.example.yaml).

## secrets.yaml

```yaml
wifi:
  - name: Home                 # optional label shown in Settings
    ssid: "MyWifi"
    password: "secret"
    ha_url: internal           # optional: internal | external | auto (default)
    static_ip:                 # optional; if present, ip, gateway, subnet and dns are all required
      ip: 192.168.1.50
      gateway: 192.168.1.1
      subnet: 255.255.255.0
      dns: 192.168.1.1
home_assistant:
  internal_url: http://192.168.1.10:8123
  external_url: https://example.ui.nabu.casa   # optional, must be https
  token: "long-lived-token"
```

`wifi` is a list of up to 4 profiles in priority order; the first one in range is used. A single
mapping (`ssid`, `password`, ...) instead of a list is still accepted. The token is stored as plain
text on the SD card, so treat the card like a key. A `connection:` section in `config.yaml` (old
format) still works if there is no `secrets.yaml`; once `secrets.yaml` exists it is an error.

## config.yaml

```yaml
version: 1
timezone: "CET-1CEST,M3.5.0,M10.5.0/3"     # optional
groups:
  downstairs_shutters: [cover.living_room_shutter, cover.kitchen_shutter]
tabs:
  - title: Living room
    landscape_columns: 4                   # optional: 4 or 5 (default 5)
    widgets:
      - entity: cover.living_room_shutter
        name: Shutter
      - group: downstairs_shutters
        name: Downstairs shutters          # required for groups
      - entity: light.living_room_ceiling
      - entity: sensor.living_room_climate
        name: Living room
        size: 2x1
        attribute: temperature
        details: [humidity, dew_point]
        decimals: 1
```

| Key | Meaning |
|---|---|
| `version` | schema version, `1` |
| `timezone` | POSIX TZ string for the clock and wake schedule; default Central Europe (`CET-1CEST,M3.5.0,M10.5.0/3`) |
| `groups` | optional map `name: [entity, ...]`; names use lowercase letters, digits and `_`; all members share one domain; up to 16 per group |
| `tabs` | 1 to 16 tabs, each with `title` and `widgets` (up to 24); a tab that overflows the screen splits into sub-pages |
| `landscape_columns` | per tab, `4` or `5` (default `5`). With 4 columns, wide widgets are 2 cells, so two covers or thermostats fit per row. Portrait always has 2 columns |
| `entity` / `group` | each widget has exactly one of them |
| `name` | label; default is the Home Assistant friendly name; required for groups |
| `size` | `WxH` within what the widget type allows (see below); rarely needed |
| `type` | override the widget type for a domain without a built-in one (e.g. `person.alice` with `type: sensor`) |
| `attribute` | sensor: show this attribute instead of the state (`a-z`, `0-9`, `_`, max 40 chars) |
| `details` | sensor: up to 2 more attributes on a details line (only shown at 2x1 and wider) |
| `unit` | sensor: unit override for the main value, max 8 characters |
| `decimals` | sensor: round numbers to 0 to 3 decimals (default: whole numbers as is, others 1 decimal) |
| `display` | `graph` draws the entity's history graph straight on the tab instead of the usual widget (sensor and binary_sensor only, a single entity). It uses the `history` options below (hours, type, statistic, period, min/max, source) and the `decimals` / `unit` of the sensor. Default size is 2x2 (a full-width block in portrait), at least 2x1, at most the grid width by 3 rows. Tapping or long-pressing it does nothing, and the rocker skips it. It is refreshed with every wake and scheduled refresh, for the tab you are on, and when you switch to its tab. Errors: other widget types, `attribute`, groups and `history: false` |
| `history` | graph options: `false` (off), a number of hours (`history: 48`), or a mapping with `hours` (1 to 720, default 24), `type` (`auto`, `line`, `bar`, `step`), `statistic` (`mean`, `min`, `max`, `sum`, `change`), `period` (`auto`, `hour`, `day`), `min` / `max` (fixed y axis, `min` < `max`), `source` (`auto`, `history`, `statistics`). All optional. `statistic` (default `mean`, `sum` for `device_class: energy`) and `period` only apply to `type: bar`, elsewhere they are ignored. Errors: `type: bar` with `source: history`, and `period: hour` bars over more than 128 hours |

Widget types come from the entity domain: `cover`, `light`, `switch`, `automation`, `climate`,
`sensor`, `binary_sensor`, `weather`, `vacuum`. Cover, climate and vacuum are wide; light, switch,
automation and binary_sensor are 1x1; sensor is 1x1 (up to 2x1); weather is 2x1 (down to 1x1).
Sizes outside the allowed range are clamped. Details are in [docs/ui/layout.md](ui/layout.md).
Config errors are reported with path and line, e.g. `tabs[0].widgets[2].entity (line 34)`. The
file may be at most 64 KB.
