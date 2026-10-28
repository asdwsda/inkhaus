# M5Paper Home Assistant Remote

An e-ink remote for Home Assistant, running on an M5Stack M5Paper (ESP32,
960x540 e-ink touch screen, 3-way rocker button, battery). Built with PlatformIO.

## Agent workflow
- The main session runs as `architect` (Opus 5.5), set in .claude/settings.json.
  It plans, writes task files, delegates, reviews and commits.
- `ui-designer` (Sonnet 5.5) writes screen specs and SVG mockups in docs/ui/.
- `firmware-dev` (Sonnet 5.5) implements one task at a time.
- `tester` (Sonnet 5.5) verifies a task against its acceptance criteria.

Flow per task: docs/tasks/NNN-*.md, then UI spec (if needed), then implement,
then test, then architect review, then commit.
PC-only tasks run in parallel (git worktrees) next to the one device task; the
device and the user's visual checks are the bottleneck.

## Layout
- `docs/ARCHITECTURE.md`: decisions and module boundaries (architect)
- `docs/tasks/`: task files and the README.md index (architect)
- `docs/ui/`: STYLE.md and per-screen specs/mockups (ui-designer)
- `src/`, `include/`, `lib/`: firmware (firmware-dev)
- `test/`: unit tests, natively run (tester, firmware-dev)

## Commands
- Build: `pio run`
- Native tests: `pio test -e native`
- Flash (device connected): `pio run -e m5paper -t upload --upload-port /dev/ttyUSB0`
- Serial: `tools/serial_capture.py --reset --seconds 15` (`pio device monitor` needs a TTY)
- Device lock (wrap every flash/reset/serial command): `tools/device_lock.sh <command...>`
- Device how-to and pitfalls: `.claude/skills/m5paper-device/SKILL.md`

## Rules
- Secrets live in `secrets.yaml` on the SD card (Wi-Fi, HA URLs, token) and in
  `include/secrets.h` (dev fallback). Both are gitignored and never read, printed or grepped by
  agents; `secrets.example.yaml` and `include/secrets.example.h` show the format.
- `config.yaml` (the dashboard: tabs, widgets, groups) holds no secrets and may be read and edited
  when the user shares it. If a `config.yaml` still contains a `connection:` section (old format),
  treat it as secret and don't read it.
- Never `git push`.
- Shell style: write variables as `"${variable}"`.
