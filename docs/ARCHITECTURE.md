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
| `ui` | lib + src | Lib: layout engine (tabs, grid, sub-pages), hit-testing, rocker focus, dirty regions. Src: widget drawing through M5GFX. |
| `input` | src + lib | Touch and rocker → abstract events (tap, long press, up/down/push). |

Dependency direction: `app` → everything; `ui` → `model`, `config`, `settings`;
`ha_client` → `model`; nothing in `lib/` depends on `src/`.

## UI layout
The UI is generated from `config.yaml`; there are no hand-placed screens except
Settings (and system screens like low battery / errors).
- **Tabs:** the user defines tabs in config, each with a title and a list of
  widgets. A tab bar switches between them; the rocker up/down also cycles tabs.
- **Widgets:** one widget type per entity domain (cover, light, switch,
  automation, climate, sensor/binary_sensor, weather, vacuum). Every widget is one
  grid row tall; toggles are 1x1, covers/climate/vacuum are wide (2 cells portrait,
  3 cells landscape). Allowed sizes per type are in docs/ui/layout.md section 3;
  config `size` can only choose within them.
- **Layout engine (`lib/ui`, pure):** given the screen size and the tab's widget
  list, flows widgets into a fixed-cell grid (row-major, first fit), splits into
  sub-pages when a tab overflows, and returns pixel rectangles for drawing,
  hit-testing and dirty-region refresh. No widget code knows absolute positions.
- **Orientation is a setting** (`portrait` default, `landscape`). Because every
  position comes from the layout engine, rotation means a different grid plus
  `setRotation()`: portrait 2x7 cells, bottom tab bar; landscape 5x4 cells, one
  combined top bar (refresh label, tabs, battery). Cells are 104 px high, 12 px gaps,
  64 px minimum touch targets (tab bar: 56 px high exception). M5Unified rotates touch
  coordinates; rocker mapping has an `invert` flag until verified on the device.
- **Fonts:** only M5GFX's built-in DejaVu at 18/24/40/56 px. Mockups are reviewed as
  PNGs rendered with DejaVu (`tools/render_mockups.sh`) and checked for overflow and
  overlap (`tools/check_mockups.py`), because SVG rendering depends on the viewer's fonts.
- Specs in docs/ui/ describe widgets in cell-relative coordinates for both grids,
  not as fixed screens.

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

## Hardware facts (measured, task 013 probe, 2026-10-27)
- ESP32-D0WDQ6-V3 rev 3.0, 240 MHz, 16 MB flash; M5Unified board id 7 (M5Paper).
- PSRAM: ~4 MB usable (4,192,123 bytes). The module carries 8 MB, but the ESP32 maps only
  4 MB directly (the rest only via himem). Plan memory with 4 MB.
- Display: M5GFX rotation 0 = 540x960 (portrait), rotation 1 = 960x540 (landscape).
  Touch coordinates follow the rotation (no extra mapping needed).
- Rocker: BtnA = up, BtnC = down, BtnB = push (physical directions as held in portrait).
  In landscape (rotation 1) BtnA points left, so "up/left = prev" holds and `invert` stays false.
  The rocker push is the only user button; the button on the back is reset (power-on via the
  rocker push and power-off behaviour on battery are verified in the power task, USB unplugged).
- `M5.begin()` took ~2.6 s with default config. Defaults to fix: `serial_baudrate = 0`
  (Serial is never started) and `clear_display = true` (a full clear on every boot,
  which would erase the frozen screen on wake). The app must set `clear_display = false`.
- EPD timings for a small region: fastest 155 ms, fast 468 ms, text 892 ms, quality 886 ms.
  The full-screen quality figure (460 ms) is suspect and gets remeasured.
- Task 015 (200x200 box, 72 px digit, 6 updates each, user-judged): fastest 166 ms leaves the
  background grey; fast 486 ms ghosts; text 910 ms ghosts heavily. Only "white fill with quality,
  then the digit with fastest" (1077 ms) is clean. In fast mode, thresholded black/white text is
  slightly cleaner than anti-aliased text.
- The panel keeps its image across a restart with both the stock start-up (~1070 ms) and a
  hand-built start-up without the IT8951 reset pulse (~69 ms); touch and partial refresh work
  after the latter.
- A hardware reset (EN/RTS, reset button, flashing, power-on) power-cycles the IT8951: about
  857 ms until BUSY releases, and its image memory is garbage. `ESP.restart()` keeps it warm
  (display start-up 78 ms, memory intact). Wake from full power-off is therefore always cold
  (~1.06 s display start-up + ~0.1 s re-render).
- The first refresh after a cold controller start takes ~430 ms even for a small box; later
  ones ~135 ms.
- Touch accuracy by hand: about 1–6 px at the center, up to ~34 px near the corners and edges.
- RTC (BM8563) was never set and reports low voltage, so time must come from NTP before
  anything relies on it.
- SD: the 32 GB card mounts fine over the shared SPI bus, and the display keeps working after.

## Flasher tool (decided 2026-10-30, tasks 061-064)
Goal: non-technical users install a pre-built release without PlatformIO.
- Stack: Rust + `espflash` crate (ESP32 flashing as a library) + `egui`/`eframe` GUI: one native binary per OS,
  no runtime, no Python. Reason: the only mature ESP flasher that is a library and links into a single static binary.
- Lives in `tools/flasher/` (own Cargo project); the firmware build is untouched.
- UI: one window: device status (auto-detected by USB VID:PID 1a86:55d4, CH9102), one big "Install" button,
  progress bar, plain-language result. Port picker only if 0 or >1 devices are found.
- Image: bootloader, partition table, boot_app0 and firmware flashed as separate segments, never a merged image from 0x0,
  so NVS (settings, Wi-Fi state, config cache) survives an update.
- Firmware source (user): downloaded from GitHub Releases at run time; version picker, latest stable preselected.
  Each release carries the segment files plus `manifest.json` (version, offsets, sizes, SHA-256); the tool checks hashes.
- Optional (user): "Upload YAML files" picks config.yaml / secrets.yaml and sends them to the SD card over USB with the
  task 020 protocol. No setup wizard: creating the files is the user's job.
- Release: GitHub Actions on a version tag builds firmware + flasher for Windows, macOS (universal) and Linux and attaches
  them to the release.
- Risks: Windows may need the CH9102 driver (detect "no device" and link to it); unsigned binaries trigger
  SmartScreen/Gatekeeper warnings (document the click-through; signing is out of scope).

## Future ideas (not planned yet)
- **BLE instead of Wi-Fi for talking to HA** (user idea, 2026-10-28; the home has many Bluetooth-capable
  Shelly devices, which HA can use as Bluetooth proxies). First assessment, to revisit after a working
  Wi-Fi version:
  - Commands: the M5Paper could broadcast short BLE advertisements (e.g. the BTHome format, which HA
    decodes natively) that the Shelly proxies forward to HA; HA automations turn them into actions.
    An advertisement burst takes tens of ms versus ~1–3 s of Wi-Fi + TLS per session, so the battery
    gain for taps could be large.
  - States back to the device are the hard part: Shelly proxies are passive receivers, so HA can't
    easily push states to the M5Paper over BLE. A likely shape is hybrid: BLE for actions, Wi-Fi for
    the less frequent state refreshes.
  - Costs: the BLE stack (~100–200 KB flash, tens of KB RAM), one HA automation per action (or a
    blueprint), no delivery confirmation for broadcast commands.

## Decisions log
- 2026-10-27: Target original M5Paper (ESP32), not M5Paper S3. Reason: the user's hardware.
- 2026-10-27: Arduino + M5Unified/M5GFX on PlatformIO. Reason: maintained M5Paper support with fast partial EPD modes; mature ecosystem.
- 2026-10-27: HA REST + long-lived token, no persistent WebSocket/MQTT. Reason: short wake sessions; an idle socket would drain the battery.
- 2026-10-30: History statistics (7 d+, bars) via a short-lived WebSocket (connect, auth, one command, close) with our own minimal RFC 6455 client over the existing TLS client. Reason: no REST endpoint for statistics; native-testable, no new dependency, no idle socket.
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
- 2026-10-27: UI fully config-driven: user-defined tabs with widget lists, laid out by a grid engine. Reason: the user wants to define what they see without reflashing.
- 2026-10-27: Orientation is a setting, portrait default. Reason: with a grid layout engine both orientations cost only a second grid definition, not a second set of screens.
- 2026-10-27: All network I/O runs in its own FreeRTOS task; the main loop owns input, UI and the idle hard cap. Reason: a stalled HTTP read can't be bounded inside HTTPClient, but it must never keep the device awake past the 120 s cap.
- 2026-10-27: Thermostats: only the target temperature is changeable; mode is display-only. Reason: user preference; no `set_hvac_mode` needed.
- 2026-10-27: Rocker long-press (TAB level) opens Settings; "refresh now" is a tap on the status bar's "updated" label (Settings has it too). Reason: user choice (option a).
- 2026-10-27: Tab chrome made compact (user feedback on first mockups); layout engine follows the revised spec in a separate task. Reason: tabs took too much screen space.
- 2026-10-27: Sleep indicator in the status bar, drawn (partial refresh of its region only) right before every power-off/deep sleep and cleared on wake. Two variants picked by the sleep plan: `touch_wake = false` (Button mode, low battery) shows "press the side button"; `touch_wake = true` shows "tap to wake". Reason: the frozen e-ink image otherwise looks awake, and the user should know how to wake it.
- 2026-10-27: Minimum touch target 64x64 px (~7 mm at 234 ppi), down from 80. Reason: user feedback that widgets were oversized; 7 mm matches common mobile touch guidance and allows ~112 px cells.
- 2026-10-27: All UI text and icons are positioned by their vertical center (M5GFX middle_* datums); specs give center lines, not top offsets. Reason: user-reported misalignment; one rule for spec, SVG (`dominant-baseline="central"`) and firmware.
- 2026-10-27: Mockups are reviewed as PNGs rendered with DejaVu and must pass `tools/check_mockups.py`. Reason: SVG viewers substitute fonts (the user saw overflow that DejaVu renders didn't have); the designer can't render, so the check is automated.
- 2026-10-27: Current UI (spec revision 4) is accepted as the baseline. Later design iterations may use the panel's full 16 grey levels (not only the 5 in STYLE.md). Reason: user direction; greyscale needs the slower GC16/quality EPD mode, so where greys are used must be weighed against refresh speed and ghosting.
- 2026-10-27: SD card (user has a 32 GB card in the device) is the user-editable store for config and later assets (icons, extra fonts, images). The device imports changed files once into internal flash (LittleFS, partition size revisited when assets exist) and normal wakes never touch the SD card. Reason: SD access costs wake time and power and shares the SPI bus with the display. The card must be FAT32 (exFAT is not enabled in the framework).
- 2026-10-27: Boot config: `Serial.begin(115200)` before `M5.begin()`, `cfg.clear_display = false`. Reason: the probe showed Serial is off by default and the default boot clears the panel, which breaks "wake into the frozen screen".
- 2026-10-27: Orientation mapping: portrait = rotation 0, landscape = rotation 1. Reason: measured; the earlier assumption that rotation 0 is landscape was wrong.
- 2026-10-27: EPD modes: tap feedback and pending/confirmed changes use `epd_fastest` (~155 ms, black/white only); greys and text-heavy regions use `epd_text`/`epd_quality` (~0.9 s). Reason: measured timings; greys cost about 6x the time.
- 2026-10-27: After every boot/wake, the full current screen is re-rendered into the IT8951's image memory without a panel refresh, and only changed regions are refreshed. Reason: a reset clears the controller's memory while the panel keeps its picture; refreshing from blank memory erased the title in the task 014 test (user report).
- 2026-10-27: Yes/no prompts on the rocker: UP = yes, DOWN = no, PUSH = skip/cancel. Push is never the "yes" answer. Reason: user feedback that a push easily tilts the rocker, so confirmations need distinct directions. (Push stays "activate" for focused UI controls; the UI spec gets this rule for dialogs.)
- 2026-10-27: Display start-up without the IT8951 reset (hand-built Panel_IT8951 + Touch_GT911 config, as in epdtest variant B), falling back to the stock M5GFX start-up if the controller doesn't answer. Reason: measured 69 ms vs 1070 ms, and the image is kept either way.
- 2026-10-27: Refresh strategy: immediate feedback with `epd_fastest` (166 ms, black/white, thresholded text); every region updated that way is tracked and gets one clean quality refresh before sleep (and on page changes). Reason: task 015 showed only a quality white pass gives a clean result, and it costs ~1 s, too slow for tap feedback.
- 2026-10-27: The app tracks what it last drew (a screen id + content hash in NVS). If the record is missing or doesn't match (first boot, another firmware ran, crash), the boot does one full quality redraw instead of partial updates. Reason: after a foreign firmware or an unknown state, partial updates over an unknown picture produce garbage.
- 2026-10-27: Panel auto-display is off (`setAutoDisplay(false)`): every draw path must call `display(x,y,w,h)` explicitly. On a cold boot, the screen is re-rendered into controller memory without a refresh, then only the changed regions are refreshed. Reason: with auto-display, the re-render triggered a full-screen fastest refresh (user-visible flash and grey veil, task 014 round 3).
- 2026-10-27: Full redraws over an unknown screen: GC16 white fill, then content with GC16. Reason: cleared the residue of earlier screens (user-confirmed); the IT8951 INIT waveform is not exposed by M5GFX.
- 2026-10-27: Every screen is rendered into an off-screen 8-bit grey canvas in PSRAM (M5Canvas, ~0.5 MB), then pushed to the panel; changed regions come from diffing the new frame against the last pushed one. Reason: exact dirty rectangles, the firmware always knows what is on the panel (cold-boot re-render becomes a plain push), and the frame can be dumped over serial as a screenshot for pixel comparison with the approved mockups.
- 2026-10-30: Modal popups: open = one quality refresh of the popup rect with real greys; close = one full GC16 refresh. Reason: partial quality refreshes are thresholded (greys lost), and the full refresh on close clears accumulated ghosting (user-checked).
- 2026-10-28: Taps send explicit target actions (light/switch `turn_on`/`turn_off` from the state the user saw, not `toggle`), and actions queued before Wi-Fi is up are sent before the initial state fetch. Reason: a toggle based on a stale frozen screen could invert the user's intent; explicit actions are idempotent, so sending first is both safe and faster.
- 2026-10-28: Refresh rules revised after the user's check (task 017): `epd_fastest` only where the region's current content is pure black/white; a region that currently holds greys (e.g. right after a GC16 full refresh) gets one `epd_quality` refresh instead. The per-tap clean-up pass is dropped; a GC16 full refresh happens on tab/page changes and when ghost debt passes the threshold (at the latest before sleep). Reason: with the colour bug fixed, fastest over black/white is clean (user-judged); fastest over GC16 greys left old text visible for 1–2 taps; the clean-up cost 1–5 s per interaction.
- 2026-10-28: Text that can change at runtime (state, values, labels of dynamic content) is drawn pure black; greys only for static decoration (dividers, never-changing captions). Reason: user doesn't mind black state text; it keeps almost every update on the 130 ms fast path. UI spec to be updated in the next ui-designer round.
- 2026-10-28: Root cause recorded: `pushGrayscaleImage(..., (uint32_t)TFT_WHITE, ...)` read the RGB565 constant as RGB888 cyan, which the IT8951 fast mode dithers to 25% black. Always pass 24-bit colours (0xFFFFFF/0x000000). This likely also caused the "grey background" of box A in task 015.
- 2026-10-28: HTTPS trust: the full Mozilla root bundle in ESP-IDF's binary bundle format, generated with ESP-IDF v4.4.7's `gen_crt_bundle.py` (vendored in certs/, matching Arduino-ESP32 2.0.17), embedded via `board_build.embed_files`. Reason: user decision; works for any public CA (own domain via Cloudflare/GTS, Nabu Casa/Let's Encrypt) without curating a list.
- 2026-10-28: Mixed groups (light/switch/automation): the group counts as "on" if any available member is on; a tap sends TurnOff to all, otherwise TurnOn. The widget shows the same aggregate. Reason: Home Assistant group semantics; "all off" is always one tap away, independent of config order (tester finding, task 019).
- 2026-10-28: Time zone as a POSIX TZ string (`setenv("TZ", …)` + `tzset()`), default `CET-1CEST,M3.5.0,M10.5.0/3` (user in Hungary), later configurable as `timezone:` in config.yaml. The UTC offset passed to `plan_sleep` is taken from `localtime` at planning time, so DST changes are handled. Reason: a fixed offset is wrong half of the year.
- 2026-10-28: NTP policy: sync when the RTC is untrusted (first boot, RTC low-voltage flag, never synced), otherwise at most once per 24 h, only opportunistically in a session that has Wi-Fi up anyway; never delays sleep (if no answer by the end of the session, try next time). The RTC is written only after a confirmed SNTP sync. Reason: the BM8563 drifts ~±2 s/day and the UI needs minute accuracy; NTP should cost ~nothing. Option for later: correct the clock from HA's HTTP `Date:` header.
- 2026-10-28: Credentials move to a separate `/secrets.yaml` (Wi-Fi, HA URLs, token); `/config.yaml` holds only the dashboard (tabs, widgets, groups, time zone) and is not secret. Transition: a `connection:` section in config.yaml still works with a log warning; once secrets.yaml exists, a `connection:` in config.yaml is rejected. Reason: user idea; agents can then read and edit the dashboard config, and dashboard changes don't resend the token.
- 2026-10-28: Sensor details (task 030/031): 1x1 keeps the name and shows no details; details only in 2x1 and wider. Reason: a nameless widget is ambiguous with several sensors on a tab (architect's recommendation, option b).
- 2026-10-28: Button wake mode uses ESP32 deep sleep with the main power hold (GPIO2) kept on, the IT8951 in standby and the GT911 asleep, waking on the rocker (ext1) and a timer for scheduled refreshes; full power-off via GPIO2 low is kept only behind a build flag until verified. Reason: on battery the device didn't wake after a power-off (user report; reset didn't help either), and M5Unified doesn't implement power-off/power-on via GPIO2 for this board. Standby current to be measured (battery test plan).
- 2026-10-29: Speed over strict confirmation (user feedback after device testing: a tap took ~3 s to switch and ~5 s to show). Taps are optimistic: the widget shows the requested state immediately (fastest refresh, no hourglass) and reverts with the failed badge only on an error. The service call's response (HA returns the changed states) confirms; no separate confirmation fetch unless the entity is missing from it. One HTTPS connection is kept alive and reused for all requests of a wake. Reason: user priority is "push and the light switches", and accepts some battery cost.
- 2026-10-29: Last known states persist across sleep and power loss (RTC memory, plus LittleFS) and are shown immediately on wake; staleness is a status-bar icon, never blanked values. The status bar always shows the link phase (connecting, fetching, online, offline, outdated). Reason: user feedback.
- 2026-10-29: Deep sleep keeps power on battery only with GPIO2 held as an RTC IO and the RTC peripheral domain on (verified: RTC boot counter keeps counting across wakes). Wake modes become three: Touch, Button, Touch & Button (user request); all use deep sleep with the power hold; Touch & Button is the old "Touch". The Settings warn icon next to the value is dropped (read as a glitch); the note text carries the battery warning.
- 2026-10-29: HA state `unknown` is a normal state (value "–", normal look, actions enabled); only `unavailable` gets the unavailable look. For covers without position feedback (e.g. uncalibrated Shelly shutters, `supported_features` without SET_POSITION, state `unknown`), a tap shows "Opening"/"Closing"/"Stopped" optimistically and a successful service call counts as confirmed. Reason: the user's shutters report `unknown` and were unusable.
- 2026-10-29: The app is named **Inkling** (user choice). The Settings sub-page with Demo mode and Debug log is called "Tools".
- 2026-10-30: Renamed to **Inkhaus** (task 066): "Inkling" is crowded (Wacom pen, 489 GitHub repos, all domains/packages taken).
- 2026-10-29: Landscape column count is a per-tab option (`landscape_columns: 4|5`, default 5); with 4 columns (223 px cells) wide widgets are 2 cells, so two covers/thermostats fit per row. Portrait stays 2 columns. Reason: user couldn't pick one grid for all content.
- 2026-10-29: License: GPL-3.0-or-later, copyright Peter Szabo (user choice). Third-party: ESP-IDF's gen_crt_bundle.py (Apache-2.0), Mozilla roots (MPL-2.0), MIT libraries fetched at build time.
- 2026-10-27: Pure logic in `lib/`, hardware in `src/`. Reason: native unit tests without the device.
