# 062: Flasher app (Rust, single binary)

Status: PC part done (no device flash yet)
Depends on: 061 (manifest format)
UI spec: in this file (desktop, not e-ink)
Needs device: yes (flash test)

## Scope
`tools/flasher/`: Rust + espflash (library) + eframe/egui. One window:
- Device line: "M5Paper connected (port)" / "Plug in the M5Paper with a USB cable" (auto-detect USB 1a86:55d4, rescan
  every 1 s); port dropdown only when 0 or >1 candidates.
- Version dropdown from the GitHub Releases API (latest stable preselected; "show pre-releases" checkbox); offline ->
  clear message + "Choose file..." for a local manifest folder/zip.
- Big "Install" button, progress bar with phase text (Downloading / Writing / Verifying / Restarting), result in plain words.
- Download to a temp dir, verify sha256 from manifest, flash the segments at their offsets (no full erase, NVS kept),
  hard-reset the device at the end.
- Errors in plain language with one suggested action (driver link on Windows, cable, "close other apps using the port").
Out of scope: YAML upload (064), CI builds (063).

## Files to touch
- tools/flasher/ (Cargo project), docs/FLASHER.md (user guide, short)

## Acceptance criteria
- [ ] `cargo build --release` and `cargo test` pass on Linux; unit tests for manifest parsing, hash check, release list parsing.
- [ ] Device: flashing a release from the picker works and keeps settings (Wi-Fi/settings survive); the device boots.
- [ ] Unplugged / wrong hash / no network each show a plain message, no crash.

## Notes
- GitHub API unauthenticated limit 60/h: fetch the release list once per start.
