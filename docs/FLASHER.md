# Inkhaus Installer (flasher)

Installs a released firmware on the M5Paper over USB. No PlatformIO or Python needed.

## Download
From the release page, pick the file for your system:
- Windows: `inkhaus-installer-windows-x86_64.exe` (double-click).
- macOS (Apple silicon and Intel): `inkhaus-installer-macos-universal.zip`; unzip, open `Inkhaus Installer.app`.
- Linux: `inkhaus-installer-linux-x86_64.tar.gz`; `tar xzf` it and run `./flasher` (needs X11 or Wayland).

The binaries are unsigned, so the first start shows a warning (see Problems).

## Use
1. Connect the M5Paper with a USB data cable and start the installer.
2. The top line says "M5Paper connected (port)". If it says "Plug in the M5Paper...", check the cable.
   A port list appears only when no or several M5Papers are found.
3. Pick a version (the newest stable one is preselected; tick "Show pre-releases" for test versions).
   Offline? Use "Choose file..." with a downloaded release zip, or the `manifest.json` from an unpacked release folder.
4. Press **Install**. Phases: Downloading, Checking files, Writing, Verifying, Restarting. Do not unplug meanwhile.

Settings (Wi-Fi, options) and the SD card are kept: only the bootloader, partition table, boot_app0 and
firmware are written; the rest of the flash (including settings storage) is not touched.

**Upload YAML files (optional).** Press "Upload YAML files..." and pick `config.yaml` and/or `secrets.yaml` (exactly
these names). Each file is sent to the SD card over USB; the M5Paper checks its size and CRC32 and its summary is shown.
File contents are never displayed or logged. After the last file the flasher sends `reboot`, so the M5Paper restarts
by itself with the new files (no extra button). Limit: 64 KB per file. Opening the port resets the board once.

## Problems
- **No device found**: use a data cable; on Windows install the CH9102 USB driver (WCH CH343 driver).
- **Port cannot be opened**: close serial monitors, Arduino IDE, PlatformIO.
- **Checksum does not match**: the download was damaged; press Install again. Nothing is written in that case.
- **Windows SmartScreen / macOS Gatekeeper warn**: the binary is unsigned. Windows: "More info", then "Run anyway".
  macOS: right-click the app, "Open", confirm. If it says the app is damaged, run
  `xattr -dr com.apple.quarantine "Inkhaus Installer.app"` once.
- **GitHub limits requests**: wait a few minutes, or use "Choose file...".

## Command line (no device needed)
`flasher --dry-run [--dir PATH|ZIP] [--version TAG] [--prerelease] [--repo OWNER/NAME] [--releases FILE]`
resolves a release, downloads it and verifies every file against `manifest.json`. Nothing is written.
`FLASHER_REPO=owner/name` changes the GitHub repository used by the window.

## Build (developers)
`cd tools/flasher && cargo build --release` (binary: `target/release/flasher`), `cargo test`.
Needs Rust. On Linux the window needs X11 or Wayland at run time; no -dev packages are needed to build.

Releases are built by `.github/workflows/release.yml` when a `v*` tag is pushed (Windows, macOS universal via
`lipo` of arm64 and x86_64, Linux glibc build on Ubuntu 22.04 without libudev).
