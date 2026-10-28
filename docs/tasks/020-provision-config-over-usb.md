# 020: Write config.yaml to the device over USB

Status: in-progress
Depends on: 017
UI spec: none
Needs device: yes (no user interaction until the final run by the user)

## Why
The user has no card reader with them. Credentials must never pass through agents, so the
user runs a tool that sends their prepared config.yaml over USB serial; the device writes it
to its own SD card.

## Scope
- **Firmware serial command** `putfile <name> <size> <crc32>` (name limited to `config.yaml` and
  `test-*.yaml`; size ≤ 64 KB). The device answers `PUT-READY`, receives exactly `size` raw bytes
  (chunked, with flow control: `PUT-ACK <offset>` every 1 KB), writes them to
  `/<name>.tmp` on the SD card, checks size + CRC32, then atomically replaces `/<name>`
  (remove old, rename tmp). Replies `PUT-OK <size> <crc32>` or `PUT-ERR <reason>`, and never
  leaves a half-written target.
- **Never log, echo or store the content anywhere else** (no serial echo, no debug dump, no
  screenshot of it). Log only name, size, CRC and the result.
- After a successful `config.yaml` write: parse it with `lib/config` and reply
  `PUT-CONFIG ok tabs=<n> widgets=<n> connection=<yes|no>` or
  `PUT-CONFIG error count=<n> first=<path> line=<n>` (paths and line numbers only; no values).
  A command `reboot` restarts (with `endBurstRead()` first) so the new config is used.
- **`tools/provision.py`** (pyserial): `tools/provision.py [--port /dev/ttyUSB0] [--name config.yaml] FILE`.
  Opens the port (the board may reset on open: wait for the boot to finish, i.e. the
  `settle_bst_end` line, before sending), sends the file, shows progress, prints the device's
  PUT-OK / PUT-CONFIG result, asks "Reboot the device now? [Y/n]", sends `reboot` and shows the
  boot result (`config_source=sd`, any config error lines). It never prints the file content,
  and refuses files > 64 KB. Uses `tools/device_lock.sh` semantics (take the lock itself via
  flock on the same lock file).
- Agents test ONLY with `config.example.yaml` sent as `--name test-provision.yaml`, never with a
  real config.yaml, and never read the user's file.

## Files to touch
- src/ (serial command handling, an SD write helper next to src/config_sd.*)
- tools/provision.py (new)
- lib/config only if a small "summary" helper (tab/widget counts) is needed, with tests

## Acceptance criteria
- [ ] `pio test -e native` and the m5paper/probe/epdtest builds pass.
- [ ] On device: sending config.example.yaml as test-provision.yaml gives PUT-OK with a matching
      CRC and a PUT-CONFIG summary; a corrupted transfer (wrong CRC flag in a test) gives PUT-ERR
      and leaves an existing target unchanged; a >64 KB file is refused by the tool.
- [ ] Code review: no path logs or echoes file content (grep the serial handler).
- [ ] The user runs `tools/provision.py <their config.yaml>` and the device boots with
      `config_source=sd`.

## Notes
- Phase 1 (parallel with the task 017 fix round): new files only (`src/provision.*`,
  `tools/provision.py`, an optional lib helper + tests); no edits to src/main.cpp or src/shot.cpp,
  no flashing. Phase 2 (after 017 is committed): hook the command into the serial handler and test
  on the device.
- The user moved their real config.yaml out of the repo (2026-10-28). Agents must still never open, grep or
  cat it; exclude it explicitly from any shell search (`grep --exclude=config.yaml`).
- Phase 1 done (firmware-dev): lib/provision (+ test/test_provision), src/provision.{h,cpp}, tools/provision.py,
  config_sd gained mount()/unmount(). Next: phase 2 hooks `provision::handleLine(line)` into shot::handle and
  tests on the device.
- Phase 1 result (2026-10-28, uncommitted, not yet reviewed): lib/provision (CRC32, whitelist,
  receiver state machine, config summary without values; 14 tests), src/provision.* (buffer in
  PSRAM, CRC before SD, tmp + read-back verify + replace), tools/provision.py (lock, waits for boot,
  ACK every 1 KB, `--dry-run`, asks before reboot). config_sd gained mount()/unmount().
  Phase 2: one line in shot::handle — `if (provision::handleLine(line)) return true;` — plus the
  device test with config.example.yaml as test-provision.yaml; the transfer blocks the loop.
- Phase 2 code done (firmware-dev, 2026-10-28, uncommitted, not yet on the device): hook in `shot::handle`;
  `provision.cpp` calls `waitDisplay()` + `endBurstRead()` before a transfer and before `reboot`'s
  `ESP.restart()`, and `boot::feed()` in the receive loop (harmless after `setupDone()`); `boot::begin()` sets
  the Serial RX buffer to 4 KB before `Serial.begin` and logs `BOOT: serial_rx_buffer=<n>`. Host test with a pty
  fake device: `python3 tools/tests/test_provision.py` (6 tests: happy, reboot, CRC mismatch, oversize, no-ACK
  timeout, bad name). m5paper build passes.
- Device test steps (architect; one `tools/device_lock.sh`-free run, provision.py locks itself):
  1. `tools/device_lock.sh pio run -e m5paper -t upload --upload-port /dev/ttyUSB0`
  2. `tools/provision.py --name test-provision.yaml config.example.yaml --yes`
     Expected: `file: config.example.yaml as /test-provision.yaml, size=<n> crc32=<c>`, `boot finished` (if the
     port open reset the board), `sent <n> / <n> bytes`, `PUT-OK <n> <c>` with the same n and c, and
     `PUT-CONFIG ok tabs=<t> widgets=<w> connection=<yes|no>`. Exit 0, no reboot (test name).
  3. Optional: also check with `tools/serial_capture.py` that the loop works afterwards (`shot` still answers) and
     that boot shows `BOOT: serial_rx_buffer=4096`.
  4. The CRC-mismatch path is covered by the host test only (the tool always sends the right CRC).
  5. A >64 KB file is refused by the tool: `head -c 70000 /dev/zero > $SCRATCH/big.yaml;
     tools/provision.py --name test-big.yaml $SCRATCH/big.yaml` -> `refused: ... (64 KB)`, exit 2.
