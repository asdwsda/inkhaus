# Config reference

Placeholder: every key of `config.yaml` and `secrets.yaml`. The tables are checked against the parser and the example files by `site/check.py`.

## config.yaml

| Key | Meaning |
|---|---|
| `version` | Schema version, `1`. |
| `timezone` | POSIX TZ string for the clock and wake schedule. |
| `sleep_screen` | Optional blocks of the sleep screen: `weather`, `calendar`, `notifications`. |
| `weather` / `calendar` / `notifications` | The `sleep_screen` blocks. |
| `groups` | Named entity lists. |
| `tabs` | One to 16 tabs. |
| `title` / `widgets` | Per tab: its label and its widgets. |
| `landscape_columns` / `portrait_columns` | Per tab: grid columns. |
| `entity` / `group` | Each widget has exactly one of them. |
| `name` | Widget label. |
| `size` | Widget size `WxH`. |
| `type` | Widget type override for a domain without a built-in one. |
| `attribute` / `details` / `unit` / `decimals` | Sensor display options. |
| `display` | `graph` draws a history graph on the tab. |
| `history` | Graph options, or `false`. |
| `hours` / `statistic` / `period` / `min` / `max` / `source` | The `history` options. |

## secrets.yaml

| Key | Meaning |
|---|---|
| `wifi` | Up to 4 Wi-Fi profiles. |
| `name` | Per profile: optional label shown in Settings. |
| `ssid` / `password` | Per profile: network name and password. |
| `ha_url` | Per profile: `internal`, `external` or `auto`. |
| `static_ip` | Per profile: `ip`, `gateway`, `subnet` and `dns`. |
| `ip` / `gateway` / `subnet` / `dns` | The `static_ip` fields. |
| `home_assistant` | `internal_url`, `external_url` and `token`. |
| `internal_url` / `external_url` / `token` | The `home_assistant` fields. |
| `connection` | Old format, an error once `secrets.yaml` exists. |
