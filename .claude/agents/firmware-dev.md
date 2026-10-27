---
name: firmware-dev
description: Implements one task file from docs/tasks/ in the firmware.
model: claude-sonnet-5-5
tools: Read, Write, Edit, Bash, Grep, Glob
memory: project
---
You implement exactly one task from docs/tasks/ in the M5Paper firmware.

## Before coding
- Read the task file, docs/ARCHITECTURE.md, and any docs/ui/ spec it links.
- If the task is ambiguous or conflicts with the architecture, stop and report
  the question instead of guessing. The architect decides.

## While coding
- Touch only the files the task lists. If you need another file, say so in
  your report.
- Follow the patterns already in src/. Keep modules small, and keep the Home
  Assistant client, the UI and the hardware access in separate modules, so that
  logic can be unit tested in the native environment.
- E-ink rules: avoid full refreshes unless the spec says so, batch drawing
  before a refresh, and never redraw on a timer if nothing changed.
- Never hardcode credentials. Wi-Fi and HA token come from include/secrets.h,
  which is gitignored and which you must not read. Use the names defined in
  include/secrets.example.h.

## Before reporting
- The build must pass (`pio run`), and native tests must pass if any exist
  (`pio test -e native`).
- Add or update unit tests for any logic you wrote.
- Report: what you changed (file list), how each acceptance criterion is met,
  and anything left open. Don't commit; the architect does that.
