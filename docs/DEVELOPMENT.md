# Development

Building, testing and the project layout. Users: see the [README](../README.md).

## Build from source

1. Install [PlatformIO](https://platformio.org/) (e.g. `pipx install platformio`).
2. Build and flash (M5Paper connected over USB):

   ```bash
   pio run                      # build
   pio test -e native           # unit tests on the PC
   pio run -e m5paper -t upload --upload-port /dev/ttyUSB0
   ```

   The first build downloads the ESP32 toolchain (a few hundred MB).
3. Alternatively send the files over USB with the helper script:

   ```bash
   python3 tools/provision.py --dry-run config.yaml          # size + CRC check, no device access
   python3 tools/provision.py config.yaml                    # sent as /config.yaml
   python3 tools/provision.py --name secrets.yaml secrets.yaml
   ```

   The tool never prints file content, only a summary such as `PUT-SECRETS ok wifi=yes ha=yes`.

For development you can put credentials in `include/secrets.h` (copy `include/secrets.example.h`);
it is only used when neither file provides a connection.

Design decisions and their reasons: [ARCHITECTURE.md](ARCHITECTURE.md). The installer: [FLASHER.md](FLASHER.md).

## Debug log

With Debug log on (Settings, Tools) each wake appends a line to `/wakelog.csv` and messages to
`/debug.log` on the SD card:

```bash
python3 tools/wakelog.py wakelog.csv
python3 tools/wakelog.py --file debug.log debug.log
```

## Repository layout

| Path | Contents |
|---|---|
| `src/`, `include/` | Device code (Arduino, M5Unified/M5GFX); `include/version.h` holds the firmware version |
| `lib/` | Pure C++17 logic with no Arduino dependencies, unit-tested on the PC: `config`, `settings`, `ha_client`, `model`, `power`, `ui`, `session`, ... |
| `test/` | Native unit tests (Unity) |
| `config.example.yaml`, `secrets.example.yaml`, `demo.yaml` | Example dashboard, example credentials, built-in demo |
| `certs/` | Root certificate bundle for HTTPS |
| `docs/ARCHITECTURE.md` | Architecture and decision log |
| `docs/tasks/` | One file per task, with acceptance criteria |
| `docs/ui/` | Style guide, layout and widget specs, SVG mockups; `png/` has the renders |
| `docs/battery-test-plan.md` | How to measure battery life |
| `tools/` | `provision.py`, `wakelog.py`, `serial_capture.py`, `screenshot.py`, `device_lock.sh`, mockup tooling |
| `probe/`, `epdtest/` | Diagnostic firmwares used to measure the hardware |

Mockups are reviewed as PNGs rendered with DejaVu Sans, the font the firmware uses.

## How it's developed

The project is built with [Claude Code](https://claude.com/claude-code) agents, defined in
`.claude/agents/`: an architect who plans, reviews and commits; a UI designer who writes the specs;
a firmware developer; and a tester. Each task goes through spec, implementation, independent
testing and review. See [CLAUDE.md](../CLAUDE.md) and [docs/tasks/README.md](tasks/README.md).
