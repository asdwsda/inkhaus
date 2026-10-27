# 001: Project skeleton

Status: in-progress
Depends on: none
UI spec: none
Needs device: no

## Scope
Set up the PlatformIO project so later tasks only add modules.
- `platformio.ini` with two envs:
  - `m5paper`: espressif32, Arduino framework, the board definition that matches
    the original M5Paper (ESP32, 16 MB flash, PSRAM enabled), a partition table
    with app space plus a LittleFS partition. Pin the platform version and all
    library versions (no floating `^` ranges on the platform).
  - `native`: host build for unit tests with Unity. It must not compile `src/`.
- Library dependencies added now, so later tasks that run in parallel don't
  all edit `platformio.ini`: M5Unified, M5GFX, ArduinoJson (v7), libyaml (C library,
  e.g. from its GitHub repo, made usable by both envs; add a `library.json` or
  build flags if needed). ArduinoJson and libyaml must be available in `native`.
- `src/main.cpp`: init M5Unified, draw "M5Paper HA remote" and the firmware version
  centered on the screen with one full refresh, log the same line to serial, then idle.
  No Wi-Fi, no sleep yet.
- Firmware version as a single define (e.g. `include/version.h`, `FW_VERSION "0.1.0"`).
- `lib/` layout convention: one folder per module (`lib/<module>/src/...`),
  pure C++17, no Arduino headers. Document the convention in `lib/README.md`.
- One smoke test in `test/test_smoke/` that compiles, runs natively, and includes
  ArduinoJson and libyaml (parse a tiny YAML string, assert one value), to prove both work on the host.

Out of scope: any real module (settings, config, HA client, power), Wi-Fi, sleep.

## Files to touch
- platformio.ini
- src/main.cpp
- include/version.h
- lib/README.md
- test/test_smoke/test_main.cpp

## Acceptance criteria
- [ ] `pio run -e m5paper` succeeds with no warnings from our own code.
- [ ] `pio test -e native` runs `test_smoke` and it passes.
- [ ] The smoke test parses YAML with libyaml and JSON with ArduinoJson on the host.
- [ ] `native` does not compile anything in `src/` (check the build output).
- [ ] Platform and library versions are pinned in `platformio.ini`.
- [ ] PSRAM is enabled for `m5paper` (`BOARD_HAS_PSRAM` or the board equivalent) and the
      partition table has a LittleFS (spiffs-type) partition.
- [ ] `lib/README.md` describes the module layout and the "no Arduino in lib/" rule.

## Notes
- On-device check comes later (device not available): screen shows the title + version.
- Report which board id and platform version you chose and why.
