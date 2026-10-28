---
name: m5paper-device
description: How to work with the real M5Paper in this project - flashing, reading serial, boot and display pitfalls, e-ink refresh rules, and running device tests with the user. Use for any task that flashes the device, reads its serial output, touches src/boot.*, display drawing or refresh code, or needs the user to look at the screen.
---

# Working with the M5Paper

Facts below are measured on this project's device (original M5Paper, ESP32-D0WDQ6-V3).
The decisions and their reasons live in docs/ARCHITECTURE.md ("Hardware facts" and the
decisions log); this skill is the how-to. When they disagree, ARCHITECTURE.md wins. Tell
the architect.

## Flashing and serial

- Port: `/dev/ttyUSB0` (CP2104). Access comes from PlatformIO's udev rules; no `dialout` needed.
- **The device is shared between agents.** Wrap every flash, reset, serial capture and
  screenshot in `tools/device_lock.sh` (it waits for the lock, default 300 s, and says who
  holds it): `tools/device_lock.sh pio run -e m5paper -t upload --upload-port /dev/ttyUSB0`.
- Flash: `pio run -e <env> -t upload --upload-port /dev/ttyUSB0`. Use `-e m5paper-debug` when
  screenshots or serial taps are needed (`shot*`, `tap`, `key`, `pend`, `mem`, `cleanup` exist only
  there); `putfile` and `reboot` are in every build.
- Serial: **`pio device monitor` does not work for agents** (it needs a TTY). Use
  `tools/serial_capture.py`:
  - `tools/serial_capture.py --reset --seconds 15` resets the board, then captures.
  - `--until 'REGEX'` stops early; `--seconds 590` with a Bash timeout of 600000 ms for long runs.
  - Without `--reset` it attaches to a running board.
- The only reset that works on this board is DTR+RTS asserted together, then both released.
  The tool does this. The esptool-style pulse does nothing.
- **Every flash and every `--reset` is a hardware reset**: it re-runs `setup()` and power-cycles
  the e-ink controller (see below). The user may be watching; keep them to what you need,
  and batch experiments into one build.
- **Opening `/dev/ttyUSB0` can reset the board** (the CP210x driver toggles DTR/RTS on open,
  whatever pyserial asks for). Commands sent while it boots are lost: keep one session open,
  wait for the boot to finish (`settle_bst_end`), then send. To send several commands, use
  one session, not one process per command.
- Separate diagnostic firmwares exist as their own envs, so the app stays untouched:
  `probe` (board facts, rocker, touch, EPD timings, SD) and `epdtest` (persistence and refresh
  modes). Flash the app back afterwards: `pio run -e m5paper -t upload ...`.

## Boot pitfalls (M5Unified 0.2.28 / M5GFX 0.2.35, pinned)

- `cfg.serial_baudrate` defaults to **0**, so M5Unified never starts Serial. Call
  `Serial.begin(115200)` first; log as `TAG: key=value`.
- `cfg.clear_display` defaults to **true**: a full-panel clear on every boot. Always false.
- Stock M5GFX autodetect **always** pulses the IT8951 reset (GPIO23, hardcoded); and
  `init_without_reset` does not avoid it. `src/boot.cpp` brings the display up by hand
  (Panel_IT8951 + Touch_GT911 via `M5.Display.setPanel`, board id set to M5Paper) and falls back
  to the stock path if GET_DEV_INFO fails. Don't call `M5.Display.init()` anywhere.
- Rotation 0 = **portrait** 540x960, rotation 1 = landscape 960x540. Touch follows rotation.
- Usable PSRAM is about **4 MB** (not 8).
- The RTC is unset until NTP sets it; never trust its time before that.
- Rocker: BtnA = up, BtnC = down, BtnB = push. In landscape BtnA points left. The rocker push
  is the only user button; the back button is reset.
- Library internals are relied on (the `_board` member, `init_impl` returning early for a known
  board). After any M5GFX/M5Unified version bump, check `BOOT: start=noreset` and
  `display_start_ms` in the log.

## The e-ink controller (IT8951) and its memory

- The panel keeps its picture without power. The **controller's image memory** is separate:
  - hardware reset / power-on / flashing: the controller power-cycles. ~0.86 s until BUSY,
    ~1.06 s display start-up, and **its memory is garbage**.
  - `ESP.restart()`: the controller stays warm. 78 ms start-up, memory intact.
- Partial refreshes compute against controller memory. If memory doesn't match the panel,
  updates come out garbled or faint. Therefore:
  - on a cold boot, re-render the **whole** current screen into memory **without** refreshing,
    then refresh only the changed boxes;
  - `lib/screen_track` records what was drawn (screen id + content hash + panel signature).
    If the record is missing or doesn't match (first boot, another firmware ran), do a full
    redraw instead.
- **Auto-display is off** (`setAutoDisplay(false)`). Every draw path must call
  `display(x, y, w, h)` and `waitDisplay()` itself. With auto-display on, any `endWrite()`
  refreshes the whole accumulated range. That caused a full-screen flash and a grey veil.
- **Reads leave the controller in a read state.** Panel_IT8951 never sends `MEM_BST_END` after
  `readRect` or the register read in `waitDisplay`; after `ESP.restart()` (warm controller)
  the next command then stalls BUSY for 4 s timeouts at a time. Rule: the **last controller
  operation before any restart or sleep is `boot::endBurstRead()`**, and every memory read is
  followed by it. Every BUSY wait in our code is bounded (300 ms) with a recovery path
  (`boot::ensureControllerReady()`: reset + init, treated as cold).
- **Watchdog:** setup runs under a 15 s task watchdog. A crash counter in RTC no-init memory
  gives a stock-fallback boot after 2 consecutive crashes, and gives up (power off, rocker
  push wakes) after 4. Feed it during long work in setup; `boot::setupDone()` ends watchdog
  coverage.

## Colours

- **Pass 24-bit colours to `pushGrayscaleImage` and friends (`0xFFFFFFu`, `0x000000u`).**
  `TFT_WHITE` is an RGB565 int (0xFFFF); cast to `uint32_t` it's read as RGB888 cyan, which
  the IT8951 fast mode dithers into 25 % black dots. That one constant caused "old text on top
  of new" and the grey fastest backgrounds for days.

## Refresh modes (measured)

| Mode | Small box | Result |
|---|---|---|
| `epd_fastest` | ~135–166 ms (first after cold start ~430 ms) | black/white only; leaves a grey background over time |
| `epd_fast` | ~486 ms | ghosts |
| `epd_text` | ~910 ms | heavy ghosting |
| `epd_quality` (GC16) | ~450 ms box, ~900 ms larger | clean |

- **Feedback:** `epd_fastest` with black/white (thresholded) content, but only where the rect's
  current content is pure black/white. Fastest can't drive GC16 greys to white (old text stays
  visible for 1–2 updates), so a rect that currently holds greys gets one `epd_quality` refresh.
- Dynamic text (states, values) is pure black, so almost every update stays on the fast path.
- No per-tap clean-up pass (user-judged unnecessary once the colour bug was fixed); a GC16 full
  refresh on tab/page changes and when ghost debt passes its threshold (at the latest before sleep).
- (Task 015's "white fill with quality, then fastest" recipe was measured before the colour
  bug was found; treat its grey-background results as suspect.)
- **Full redraw over an unknown screen:** a GC16 white fill, then the content in GC16. It removes
  the residue of earlier screens.
- Never refresh when nothing changed. Batch drawing before one `display()`.

## Running a device test with the user

- The user must know **before** anything happens on the screen: say what to watch for, wait
  for "ready", then reset. Never reset first and ask afterwards.
- Answers via rocker: **UP = yes, DOWN = no, PUSH = skip**. Never use push for "yes": a push
  easily tilts the rocker.
- Prompts in `fonts::DejaVu40` (readable at arm's length), with a hint line in DejaVu24.
- Persist the test step in NVS, so a test survives its own restarts. Clear stale state at the start.
- Log every answer as `TAG: key=value`. **Write down what the user says beyond yes/no**: in
  task 015 the real findings (grey background, residue) were only in the user's words.
- A question like "Is box X clean?" stores the answer, not "ghosting". Name keys so they
  can't be read inverted.

## Fonts

- Only M5GFX's built-in DejaVu at 18/24/40/56 (72 for diagnostics). Never the default bitmap
  font scaled up: it looks bad on the panel.
