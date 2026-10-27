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

## Layout
- `docs/ARCHITECTURE.md`: decisions and module boundaries (architect)
- `docs/tasks/`: task files and the README.md index (architect)
- `docs/ui/`: STYLE.md and per-screen specs/mockups (ui-designer)
- `src/`, `include/`, `lib/`: firmware (firmware-dev)
- `test/`: unit tests, natively run (tester, firmware-dev)

## Commands
- Build: `pio run`
- Native tests: `pio test -e native`
- Flash (device connected): `pio run -t upload`
- Serial: `timeout 30 pio device monitor`

## Rules
- Secrets live in `include/secrets.h` (dev fallback) and in the `connection:`
  section of the SD card's `config.yaml`. Both are gitignored and never read by
  agents. `include/secrets.example.h` and `config.example.yaml` show the format.
- Never `git push`.
- Shell style: write variables as `"${variable}"`.
