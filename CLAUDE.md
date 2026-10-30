# M5Paper Home Assistant Remote

An e-ink remote for Home Assistant, running on an M5Stack M5Paper (ESP32,
960x540 e-ink touch screen, 3-way rocker button, battery). Built with PlatformIO.

## Agent workflow

### Roles

- The main session runs as `coordinator` (Sonnet 5.5).
  It manages the workflow, performs routine planning, writes task
  files, delegates work, reviews results, and commits changes.
- `architect` (Opus 5.5) handles complex architectural planning,
  significant technical decisions, and critical design reviews.
- `ui-designer` (Sonnet 5.5) writes screen specs and SVG mockups
  in docs/ui/.
- `firmware-dev` (Sonnet 5.5) implements one task at a time.
- `tester` (Sonnet 5.5) independently verifies changes against
  their acceptance criteria.

### Planning and architectural escalation

The coordinator is the default planner.

Plan routine and moderately complex tasks directly with Sonnet.
Do not invoke the architect merely because a new task begins.

Invoke `architect` when:
- Introducing a new subsystem or changing fundamental architecture.
- Making significant memory, performance, or power tradeoffs.
- Designing complex display refresh, sleep/wake, or persistence logic.
- Changing interfaces shared by multiple subsystems.
- Investigating difficult bugs after ordinary debugging fails.
- A design decision could cause extensive rework if incorrect.

When invoking the architect:
- Provide a focused question and relevant file references.
- Request concrete recommendations and tradeoffs.
- Prefer one architectural consultation per task.
- Record accepted decisions in the task file or architecture docs.
- Do not request repeated approval of routine implementation details.

### Task workflow

For each nontrivial task:

1. Coordinator creates docs/tasks/NNN-*.md with scope,
   acceptance criteria, and affected components.
2. Coordinator determines whether architectural consultation
   is necessary. If so, consult architect before implementation.
3. If UI work is required, invoke ui-designer to produce specs
   and mockups in docs/ui/.
4. Delegate implementation to firmware-dev with the approved
   task file and relevant design references.
5. Run automated builds and checks.
6. Invoke tester to independently verify acceptance criteria.
7. Coordinator reviews the diff, test results, and any findings.
8. Invoke architect for final review only when the change is
   architecturally critical or deviates from an approved design.
9. Coordinator commits the completed task.

For trivial changes, the coordinator may implement and validate
directly without creating a separate task or spawning agents.

### Delegation and token efficiency

- Invoke agents only when their expertise is needed.
- Avoid duplicate file exploration across agents.
- Pass task file paths and relevant references instead of
  copying large documents into prompts.
- Agents must return concise summaries, changed file paths,
  test results, and blockers.
- Do not return full source files or lengthy build logs.
- Avoid repeated agent review loops without new evidence.
- Do not spawn nested agents unless explicitly required.
- Prefer deterministic checks over AI-based checks.
- Keep the main session focused on one feature or task group.
- Start a fresh session when switching to unrelated work.

### Output style (user, 2026-10-30: keep usage low)

- All roles: be terse. No preambles, recaps of the request, or
  restating what the reader already knows.
- Replies to the user: a few short lines or a small list; details
  only when asked. Questions to the user: one line each.
- Agent reports: at most ~20 lines; full evidence goes to a
  scratchpad file, referenced by path.
- Prompts to agents: point to files, don't paste their content.
- Open questions to the user: keep one running numbered list and
  show it at the end of each reply (drop answered items, keep
  numbering stable); never scatter questions through the text.
  When asking, give a recommended default so the user can just say "ok".

### Parallel execution

PC-only tasks may run in parallel using isolated git worktrees,
alongside one device-dependent task.

Only one task may require exclusive access to the M5Paper
device at a time.

Device testing and the user's visual verification remain the
primary scheduling constraints.

Avoid additional parallel agents when their work depends on
the same files, device, or unfinished implementation.

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
