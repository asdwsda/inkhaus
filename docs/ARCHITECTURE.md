# Architecture

Owned by the architect. Status: first draft (2026-10-27), hardware not yet
available, so power figures below are estimates to be measured.

## Goals
- Battery-powered Home Assistant remote on the **original M5Paper** (ESP32-D0WDQ6-V3,
  8 MB PSRAM, 16 MB flash, IT8951 e-ink controller, GT911 touch, BM8563 RTC,
  SHT30, 1150 mAh battery, 3-way rocker).
- **Battery life is the top priority.** Target: ~1 month per charge with default
  settings.
- Control use cases (milestone 1-2):
  - Shutters (covers): up / down / stop, individually and in groups.
  - Lights and switches: toggle.
  - Automations: enable / disable.
  - Thermostats (climate): view current temperature and humidity, change target.
  - Outside temperature (sensor / weather): view.
  - Robot vacuum(s): start / pause / return to dock, show status.
- Later: dashboard functions (charts, history, aggregated states).
- Later: publishable app, so nothing user-specific may be compiled in, and
  remote (non-LAN) HA access must work.
- On-device settings screen: wake mode, refresh interval, idle timeout (more later).

## Stack
- PlatformIO, Arduino framework on espressif32, **M5Unified + M5GFX** for display,
  touch, buttons, RTC and power. Exact platform/core version gets pinned in task 001.
- **ArduinoJson** for HA payloads and the internal config representation.
- **YAML config parsed with libyaml** (plain C, builds for both device and
  `native`, used directly without an Arduino wrapper), converted into an
  ArduinoJson document, then into the typed config model. Reuses one JSON code path.
- Two PlatformIO envs: `m5paper` (device) and `native` (host unit tests).

## Home Assistant connection
- **REST API with a long-lived access token** (`Authorization: Bearer`).
  Request/response fits short wake sessions; no persistent socket while idle.
- **State fetch: one `POST /api/template` call** that renders a compact JSON of
  only the entities in the layout (state + the few attributes each widget needs).
  One small response instead of `/api/states` (can be hundreds of KB) or N
  per-entity requests. To verify early: token permissions for the template endpoint.
  Fallback: per-entity `GET /api/states/<id>` over one keep-alive connection.
- **Actions: `POST /api/services/<domain>/<service>`** with `entity_id` as a
  list, so groups work both as HA group entities and as device-side lists.
- **Local-first, remote fallback** (like the HA companion app): config holds an
  `internal_url` (http, LAN) and an optional `external_url` (https, e.g. Nabu Casa).
  Try internal first; on failure use external and remember which worked
  (RTC memory) for the next wake.
- **TLS:** mbedTLS with the ESP-IDF CA bundle; TLS session resumption to cut
  handshake cost on later wakes. TLS is only used for external URLs.
- **History (dashboard, later):** `/api/history/period` with
  `minimal_response` and `significant_changes_only`, downsampled on device.
  Rendered on device first; if measured cost is too high, switch to server-side
  rendered images. The `ui` chart widget hides that choice from the other modules.
- **Wi-Fi fast reconnect:** cache BSSID + channel (and optionally static IP from
  config) so reconnect takes well under 1 s instead of a full scan + DHCP.

## Configuration and settings
Two separate stores, different owners:

| Store | Where | Who edits | Contents |
|-------|-------|-----------|----------|
| `config.yaml` | SD card | user (PC now, remotely later) | connection (Wi-Fi, HA URLs, token), pages, widgets, entity groups |
| settings | NVS (`Preferences`) | user, on the device's settings screen | wake mode, refresh interval, idle timeout, ... |

- Config is parsed only when the file changes (size + mtime/hash). The parsed
  model is cached in flash (LittleFS) so a normal wake doesn't parse YAML.
- **Credentials (Wi-Fi, HA token) live in the `connection:` section of
  `config.yaml`.** `include/secrets.h` is a development fallback only (used when
  the SD card has no connection section). Published builds read connection data
  from SD. Known trade-off: the token is plaintext on the SD card; encryption or
  on-device provisioning can come later.
- A real `config.yaml` is gitignored and never read by agents (same rule as
  `secrets.h`). `config.example.yaml` in the repo root documents the schema and
  is used by tests.
- Last known entity states are cached (flash), so the UI can redraw a page
  immediately after a wake and fill in fresh values when they arrive.

## Modules
Pure logic lives in `lib/` with no Arduino includes, so it runs under `native`.
Hardware wrappers are thin and live in `src/`.

| Module | Kind | Responsibility |
|--------|------|----------------|
| `app` | src | Boot: read wake reason, decide the session (interactive / scheduled refresh / settings), run the main loop, decide when to sleep. |
| `power` | src + lib | Shutdown vs deep sleep, wake-source setup, RTC alarm, battery voltage. Lib part: sleep policy (given settings + reason, what next). |
| `net` | src | Wi-Fi connect with BSSID/channel cache, timeouts. |
| `ha_client` | lib + src | Lib: build template request, parse responses, build service calls. Src: HTTP(S) transport behind an interface, local/remote fallback. |
| `config` | lib + src | Lib: YAML/JSON → typed config model, validation errors. Src: SD/LittleFS IO and cache. |
| `settings` | lib + src | Lib: typed settings with defaults and validation, over a key-value interface. Src: NVS backend. |
| `model` | lib | Entity state store: values, availability, last-updated, pending commands. |
| `ui` | lib + src | Lib: pages, widgets, hit-testing, focus for rocker navigation, dirty regions. Src: drawing through M5GFX. |
| `input` | src + lib | Touch and rocker → abstract events (tap, long press, up/down/push). |

Dependency direction: `app` → everything; `ui` → `model`, `config`, `settings`;
`ha_client` → `model`; nothing in `lib/` depends on `src/`.

## Power and refresh strategy
Average current budget for 1 month: 1150 mAh × ~0.8 usable / 720 h ≈ **1.3 mA**.

**Wake mode (setting):**
- `button` (**default**): full power-off between sessions (release the M5Paper
  power-hold line). Wake by the rocker/power button or a BM8563 RTC alarm.
  Off current should be in the tens of µA.
- `touch`: ESP32 deep sleep with the GT911 kept powered and its INT line as a
  wake source. Expected to cost much more at rest (est. ~1-2 mA on this board);
  the settings screen will warn that it shortens battery life. Measure on hardware.
- To verify on hardware: exactly which buttons can power the board back on
  from full power-off.

**Sessions:**
- Interactive: on wake, the screen already shows the last page (e-ink holds it).
  Touch and buttons are live immediately; Wi-Fi + state fetch start in the
  background; taps made before the connection is up are queued and sent once
  connected. Go back to sleep after `idle_timeout` (default 20 s) without input.
- Scheduled refresh (`refresh_interval`, default 60 min, `off` allowed): wake on
  RTC alarm, fetch states, redraw only the changed widgets, power off. No touch
  init, so it's as short as possible.
- Rough estimate with defaults: hourly refresh ≈ 0.1 mAh × 24 ≈ 3 mAh/day;
  ~20 interactive sessions/day ≈ 10 mAh/day → ≈ 400 mAh/month, leaving margin
  for self-discharge and estimate error. Measure once hardware is back.

**E-ink refresh:**
- Tap feedback and value changes: partial refresh of the widget's region only,
  with the fast M5GFX EPD mode.
- Page change: full-quality refresh of the whole screen.
- Ghosting: full refresh after N partial updates (N tuned on device) and on
  every scheduled refresh that changes something.
- Never redraw if nothing changed. Show "last updated" wherever staleness matters.
- EPD power is turned off before sleeping.

## Decisions log
- 2026-10-27: Target original M5Paper (ESP32), not M5Paper S3. Reason: the user's hardware.
- 2026-10-27: Arduino + M5Unified/M5GFX on PlatformIO. Reason: maintained M5Paper support with fast partial EPD modes; mature ecosystem.
- 2026-10-27: HA REST + long-lived token, no persistent WebSocket/MQTT. Reason: short wake sessions; an idle socket would drain the battery.
- 2026-10-27: Fetch states with one `/api/template` call. Reason: one small response for only the needed entities.
- 2026-10-27: Local-first URL with optional external HTTPS fallback. Reason: fast and cheap at home, still works remotely for a published app.
- 2026-10-27: Layout/connection in `config.yaml` on SD; user settings in NVS. Reason: no reflash to change layout; device-side settings survive SD edits.
- 2026-10-27: Wake mode is a user setting; default `button` with full power-off. Reason: touch wake keeps the GT911 powered and probably breaks the 1-month target.
- 2026-10-27: Default refresh interval 60 min, adjustable. Reason: fits the battery budget with margin.
- 2026-10-27: Charts rendered on device first, server-side rendering as fallback. Reason: self-contained; switch only if measured cost is too high.
- 2026-10-27: Credentials in `config.yaml` on SD, `secrets.h` only as dev fallback. Reason: a published firmware can't have credentials compiled in. Accepted: plaintext token on SD for now.
- 2026-10-27: libyaml used directly instead of an Arduino YAML wrapper. Reason: plain C builds for `native` tests too.
- 2026-10-27: C++17 (`gnu++17`) in both envs. Reason: `lib/` uses std::optional etc.; the toolchain (GCC 8.4) supports it, the core just defaults to gnu++11.
- 2026-10-27: Idle timer runs on a monotonic clock (`millis()`), RTC alarms on wall time. Reason: an NTP/RTC step would otherwise stretch or cut the awake time.
- 2026-10-27: Pure logic in `lib/`, hardware in `src/`. Reason: native unit tests without the device.
