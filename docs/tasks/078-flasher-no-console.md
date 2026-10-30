# 078: Windows installer opens no console window

Status: in-progress
Depends on: 069
UI spec: none
Needs device: no (Windows check by the user)

## Scope
User report (v1.2.0, 2026-10-30): on Windows, the installer opens an empty cmd window behind the app.
Build the release binary for the Windows GUI subsystem (`windows_subsystem = "windows"` in release
builds). The command line mode (`--dry-run` etc.) must still print when started from a terminal:
attach to the parent console (`AttachConsole(ATTACH_PARENT_PROCESS)`) when CLI arguments are given.

## Files to touch
- tools/flasher/src/main.rs, tools/flasher/Cargo.toml (Windows-only dependency if needed), docs/FLASHER.md if behavior notes change.

## Acceptance criteria
- [ ] `cargo test --release` and `cargo build --release` pass on Linux; the Windows-only code compiles
  (`cargo check --target x86_64-pc-windows-gnu` if the target is available, else CI on the next tag).
- [ ] Windows (user): double-clicking the .exe shows only the app window; `flasher.exe --dry-run` in cmd
  still prints its output.
- PC part done 2026-10-30 (main.rs only); released in 1.2.1, Windows check by the user pending.
