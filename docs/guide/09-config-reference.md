# Config reference

Every key of `config.yaml` and `secrets.yaml`, with its values, default and effect. The key tables are checked against the program code by `site/check.py`. If you are new, read [Getting started](01-getting-started.md) first.

Both files live in the root of the SD card. Examples to copy: [`config.example.yaml`](https://github.com/asdwsda/inkhaus/blob/main/config.example.yaml) and [`secrets.example.yaml`](https://github.com/asdwsda/inkhaus/blob/main/secrets.example.yaml). Mistakes are reported with their place, for example `tabs[0].widgets[2].entity (line 34)`: see [Settings, Reading the Config row](06-settings.md#reading-the-config-row). The file may be at most 64 KB.

## config.yaml

Your dashboard. It contains no secrets.

```yaml
version: 1
timezone: "CET-1CEST,M3.5.0,M10.5.0/3"
groups:
  downstairs_shutters: [cover.living_room_shutter, cover.kitchen_shutter]
tabs:
  - title: Living room
    landscape_columns: 4
    portrait_columns: 3
    widgets:
      - entity: light.living_room_ceiling
      - group: downstairs_shutters
        name: Downstairs shutters
      - entity: sensor.living_room_climate
        name: Living room
        size: 2x1
        attribute: temperature
        details: [humidity, dew_point]
        decimals: 1
```

### Top level

| Key | Values | Default | Effect |
|---|---|---|---|
| `version` | `1` | required | The file format version. |
| `timezone` | POSIX TZ string | `CET-1CEST,M3.5.0,M10.5.0/3` | Time zone for the clock and the wake schedule. |
| `sleep_screen` | block, see below | none | Optional blocks of the [sleep screen](08-sleep-screen.md). |
| `groups` | map of `name: [entities]` | none | Named entity lists. Names use `a-z`, `0-9`, `_`. All members share one kind. Up to 16 each. |
| `tabs` | list of tabs | required | 1 to 16 tabs. |

### sleep_screen

| Key | Values | Default | Effect |
|---|---|---|---|
| `weather` | a `weather.` entity id | none | Adds the weather block. |
| `calendar` | a `calendar.` entity id | none | Adds the next event block. |
| `notifications` | `true` or `false` | `false` | Adds the notifications block (count and newest title). |

### Tabs

| Key | Values | Default | Effect |
|---|---|---|---|
| `title` | text | required | The tab label. |
| `widgets` | list of widgets | required | Up to 24 per tab. A tab that does not fit splits into pages. |
| `landscape_columns` | `4` or `5` | `5` | Grid columns in landscape. With 4, wide widgets are 2 cells. |
| `portrait_columns` | `2`, `3` or `4` | `2` | Grid columns in portrait. |

### Widgets, all types

Each widget has exactly one of `entity` or `group` (except `type: notifications` and `type: energy`, which have neither).

| Key | Values | Default | Effect |
|---|---|---|---|
| `entity` | entity id, `domain.object_id` | none | The Home Assistant thing. Supported domains: `cover`, `light`, `switch`, `automation`, `climate`, `sensor`, `binary_sensor`, `weather`, `vacuum`, `media_player`. |
| `group` | a name from `groups` | none | A widget for all members of the group. Needs `name`. |
| `name` | text | the Home Assistant name | The label. Required for groups. Default `Alerts` for notifications. |
| `size` | `WxH` | depends on the type | See [Widgets, Sizes](04-widgets.md#sizes). Sizes a type does not allow are corrected. |
| `type` | `sensor`, `cover`, `light`, `switch`, `automation`, `climate`, `binary_sensor`, `weather`, `vacuum`, `media_player`, `notifications`, `energy` | from the entity | Pick the widget type yourself, for example `type: sensor` for `person.alice`. `notifications` and `energy` need no entity. |

### Sensor options

| Key | Values | Default | Effect |
|---|---|---|---|
| `attribute` | `a-z`, `0-9`, `_`, up to 40 characters | the state | Show this attribute instead of the state. |
| `details` | list of up to 2 attribute names | none | A details line (only at 2x1 and wider). |
| `unit` | text, up to 8 characters | from Home Assistant | Replaces the unit of the main value. |
| `decimals` | `0` to `3` | whole numbers as is, others 1 decimal | Rounds numbers. |

### Weather option

| Key | Values | Default | Effect |
|---|---|---|---|
| `forecast` | `true` or `false` | `false` | Adds the next 3 days to a weather widget. Single entity only. Needs a wide tile; the size is raised. |

### Energy widget

`type: energy`, no `entity` or `group`. At least one of `usage` and `solar` is needed. Size is always 2x1. `name` and `size` are accepted but not shown.

| Key | Values | Default | Effect |
|---|---|---|---|
| `usage` | sensor id | none | The consumption sensor, the top row (`Used`). A running total (`total_increasing`, such as a Shelly 3EM total) works: the widget shows the change since local midnight. |
| `solar` | sensor id | none | The solar production sensor, the second row (`Solar`). Same rules. |
| `history` | `false` | on | `history: false` switches the long press popup off. |

Both sensors need long-term statistics in Home Assistant (any sensor with a `state_class` has them). `Wh` is shown as `kWh`.

### Graph and history

| Key | Values | Default | Effect |
|---|---|---|---|
| `display` | `graph` or `default` | `default` | `graph` draws the history right on the tab. Sensor and binary sensor only, single entity, no `attribute`, not with `history: false`. |
| `history` | `false`, hours (`history: 48`), or a block | on, 24 hours | Options of the graph and of the long-press popup. `false` switches both off. |
| `hours` | `1` to `720` | `24` | How far back. |
| `statistic` | `mean`, `min`, `max`, `sum`, `change` | `mean` (`sum` for energy sensors) | What a bar shows. Only with `type: bar`. |
| `period` | `auto`, `hour`, `day` | `auto` | Bar width. Only with `type: bar`. Hourly bars over more than 128 hours are an error. |
| `min` | number below `max` | automatic | Lowest value of the y axis. |
| `max` | number above `min` | automatic | Highest value of the y axis. |
| `source` | `auto`, `history`, `statistics` | `auto` | Where the data comes from. `type: bar` with `history` is an error. |

Inside `history:`, `type` has its own values: `auto`, `line`, `bar` and `step` (default `auto`).

```yaml
- entity: sensor.energy_today
  display: graph
  history:
    hours: 168
    type: bar
    statistic: sum
    period: day
```

### Notes

- The widget types and their sizes are explained with pictures in [Widgets](04-widgets.md).
- Unknown keys inside a widget are ignored without an error, so a typo such as `entty:` shows up as "one of 'entity' or 'group' is required".
- Old format: a `connection:` section in `config.yaml` still works if there is no `secrets.yaml`. Once `secrets.yaml` exists, it is an error.

## secrets.yaml

Private: your Wi-Fi password and the Home Assistant token. Keep it off the internet. It is stored as plain text on the card, so treat the card like a key.

```yaml
wifi:
  - name: Home
    ssid: "MyWifi"
    password: "secret"
    ha_url: internal
    static_ip:
      ip: 192.168.1.50
      gateway: 192.168.1.1
      subnet: 255.255.255.0
      dns: 192.168.1.1
  - name: Phone hotspot
    ssid: "MyPhone"
    password: "secret2"
    ha_url: external
home_assistant:
  internal_url: http://192.168.1.10:8123
  external_url: https://example.ui.nabu.casa
  token: "long-lived-token"
```

| Key | Values | Default | Effect |
|---|---|---|---|
| `wifi` | list of 1 to 4 profiles | required | In priority order: the first one in range is used. A single profile without the list dash is also accepted. |
| `name` | 1 to 20 characters, unique | `Network 1`, `Network 2`, ... | The label shown in Settings. |
| `ssid` / `password` | text | `ssid` required, `password` optional (open network) | Network name and password, inside quotes. Each profile needs a different SSID. |
| `ha_url` | `internal`, `external`, `auto` | `auto` | Which address to use on this network. `external` needs `external_url`. |
| `static_ip` | block | DHCP | A fixed IP address. All four fields are then required. |
| `ip` / `gateway` / `subnet` / `dns` | IPv4 addresses | none | The `static_ip` fields. |
| `home_assistant` | block | required | The Home Assistant address and token. |
| `internal_url` / `external_url` / `token` | text | `internal_url` and `token` required | `internal_url` starts with `http://` or `https://`. `external_url` is optional and must start with `https://`. The token is a long-lived access token (see [Getting started](01-getting-started.md#step-2-create-a-home-assistant-token)). |
| `connection` | | | Old format: it is an error once `secrets.yaml` exists. |
