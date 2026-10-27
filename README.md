# M5Paper Home Assistant Remote

A battery-powered e-ink remote control for [Home Assistant](https://www.home-assistant.io/),
running on the original **M5Stack M5Paper** (ESP32, 960×540 e-ink touch screen, 3-way rocker,
1150 mAh battery).

You decide what it shows with a `config.yaml` on the SD card. You define tabs, and the
entities (shutters, lights, switches, automations, thermostats, sensors, weather, robot
vacuums) that appear in each one. Battery life comes first: the goal is about **one month
per charge** with the default settings.

| Portrait | Landscape |
|---|---|
| ![Portrait mockup](docs/ui/png/tab-portrait.png) | ![Landscape mockup](docs/ui/png/tab-landscape.png) |

*Design mockups. The firmware doesn't draw these screens yet; see [Status](#status).*

## How it works

- **Asleep almost all the time.** E-ink keeps its image with no power. The device wakes
  on the side button (or a tap, in touch-wake mode), talks to Home Assistant, updates
  only the parts of the screen that changed, and goes back to sleep after a short idle
  timeout. A small icon in the status bar shows how to wake it.
- **Home Assistant REST API** with a long-lived access token. All states come from one
  `POST /api/template` call that returns compact JSON for just the configured entities.
  Actions are normal service calls.
- **Local first, remote fallback.** It tries the LAN URL first and falls back to an
  optional external HTTPS URL (e.g. Nabu Casa), remembering whichever one worked.
- **Optional scheduled refresh** (off / 15 / 30 / 60 / 120 / 240 min), aligned to the
  clock, so the frozen screen doesn't get too stale.
- **On-device Settings:** orientation (portrait / landscape), wake mode (button /
  touch), refresh interval and idle timeout. Open it with a long press of the rocker
  or a tap on the battery indicator.

Design decisions and the reasons behind them are in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Status

Work in progress. The device is not usable as a remote yet.

| Done (unit-tested on the PC) | Not done yet |
|---|---|
| Project skeleton, pinned toolchain | Drawing the widgets on the e-ink panel |
| Settings (stored in NVS) | Wi-Fi, sleep/wake and power handling on the device |
| `config.yaml` parser and validation | Settings screen |
| Home Assistant client (states, service calls, fallback) | HTTPS certificate bundle (task 008) |
| Power policy (what to do on each wake, sleep plan) | Any on-device test: it hasn't run on hardware yet |
| UI layout engine (grid, tabs, hit-testing, rocker navigation) | |

The task list and its progress are in [docs/tasks/README.md](docs/tasks/README.md).
Right now the firmware only shows a "hello" screen with the version number.

## Hardware

- M5Stack M5Paper (the original ESP32 version, not the M5Paper S3)
- microSD card for `config.yaml`

## Building

Requires [PlatformIO](https://platformio.org/) (e.g. `pipx install platformio`).

```bash
pio run                      # build the firmware
pio test -e native           # run the unit tests on the PC
pio run -t upload            # flash (M5Paper connected over USB)
timeout 30 pio device monitor   # serial log
```

The first build downloads the ESP32 toolchain (a few hundred MB).

## Configuration

1. Copy [`config.example.yaml`](config.example.yaml) to the **root of the SD card** as
   `config.yaml`.
2. Fill in your Wi-Fi details, your Home Assistant URL(s), and a long-lived access token
   (Home Assistant → your profile → Security → Long-lived access tokens).
3. Define your tabs and widgets. The widget type comes from the entity's domain:

```yaml
version: 1
connection:
  wifi:
    ssid: "MyWifi"
    password: "secret"
  home_assistant:
    internal_url: http://192.168.1.10:8123
    external_url: https://example.ui.nabu.casa   # optional, https only
    token: "long-lived-token"

groups:                                # optional: control several entities at once
  downstairs_shutters: [cover.living_room_shutter, cover.kitchen_shutter]

tabs:
  - title: Living room
    widgets:
      - entity: cover.living_room_shutter
        name: Shutter
      - group: downstairs_shutters
        name: Downstairs shutters
      - entity: light.living_room_ceiling
      - entity: climate.living_room
      - entity: sensor.outside_temperature
        name: Outside
```

Supported domains: `cover`, `light`, `switch`, `automation`, `climate`, `sensor`,
`binary_sensor`, `weather`, `vacuum`. Config errors are reported with their path and
line number, e.g. `tabs[0].widgets[2].entity (line 34)`.

**Your `config.yaml` contains credentials.** It is gitignored; never commit it. The token
is stored as plain text on the SD card, so treat the card like a key.

For development you can instead put credentials in `include/secrets.h` (copy
`include/secrets.example.h`; also gitignored). It is only used when `config.yaml` has no
`connection:` section.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | Device-specific code (Arduino, M5Unified/M5GFX) |
| `lib/` | Pure C++17 logic with no Arduino dependencies, unit-tested on the PC: `config`, `settings`, `ha_client`, `model`, `power`, `ui` |
| `test/` | Native unit tests (Unity) |
| `docs/ARCHITECTURE.md` | Architecture and decision log |
| `docs/tasks/` | One file per task, with acceptance criteria |
| `docs/ui/` | Style guide, layout and widget specs, SVG mockups; `png/` has the reference renders |
| `tools/` | Mockup rendering (`render_mockups.sh`), overflow/overlap check (`check_mockups.py`), flow GIF builder (`make_flow_gif.sh`) |

Mockups are reviewed as PNGs rendered with DejaVu Sans, the font the firmware uses.
SVG viewers substitute fonts and show misleading layouts.

## How it's developed

The project is built with [Claude Code](https://claude.com/claude-code) agents, defined
in `.claude/agents/`: an architect who plans, reviews and commits; a UI designer who
writes the specs; a firmware developer; and a tester. Each task goes through
spec → implementation → independent testing → review. See [CLAUDE.md](CLAUDE.md).
