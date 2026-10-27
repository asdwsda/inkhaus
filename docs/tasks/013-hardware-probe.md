# 013: Hardware probe firmware

Status: done
Depends on: 001
UI spec: none (plain diagnostic screens, DejaVu fonts)
Needs device: yes (user follows on-screen prompts)

## Scope
A separate diagnostic firmware that answers the open hardware questions, so later
tasks build on facts. It is a separate PlatformIO env, so the main app stays untouched.
- New env `[env:probe]` in platformio.ini: same as `m5paper` (extends it), but compiles
  only `probe/` (e.g. `build_src_filter = -<*> +<../probe/>`). `pio run -e m5paper` must
  stay unchanged.
- `probe/main.cpp`. Every result goes to serial as one line `PROBE: <key>=<value>`, and
  each step is also shown on screen in `fonts::DejaVu24`/`DejaVu40` (also a preview of
  the real fonts).
  1. **Serial bring-up:** `Serial.begin(115200)` as the very first statement, print
     `PROBE: stage=pre_begin`, then `M5.begin()`, then `PROBE: stage=post_begin`, with
     `Serial.flush()` after each. Also report what M5Unified's `cfg.serial_baudrate`
     defaults to. This explains why the task 001 firmware printed nothing after the ROM log.
  2. **Facts:** `M5.getBoard()` (numeric + name), PSRAM size, free heap, flash size, CPU
     MHz, reset reason, battery voltage (`M5.Power.getBatteryVoltage()`), battery level,
     charging state if available, RTC date/time and whether it was ever set, SHT30
     temperature/humidity if M5Unified exposes it (else skip), and display width/height
     at rotation 0 and 1.
  3. **Start gate:** the screen says "Hardware probe – press the rocker (push) to start".
     Wait without a timeout. Log every button event at all times as
     `PROBE: btn=<A|B|C|PWR> event=<pressed|released|long>`.
  4. **Rocker mapping:** prompt "Push the rocker UP", then "DOWN", then "PUSH IN", each
     waiting for one press. Log `PROBE: rocker_up=<A|B|C>` etc. Then ask the user to press
     the separate side/power button if there is one, with a 15 s timeout, and log what fires.
  5. **Touch, landscape (rotation 0) and portrait (rotation 1):** draw a target circle in
     each corner (40 px from the edges) and the center, one at a time ("Tap the circle").
     Log the reported touch x,y against the target center, and the error in px. After
     the five points, log `PROBE: touch_rot<0|1>_max_err=<px>`.
  6. **E-ink modes:** draw a test pattern of 16 grey bars at rotation 0 with
     `epd_quality`, then redraw a small region with `epd_fastest`, `epd_fast` and `epd_text`.
     Measure each `display()`+`waitDisplay()` duration and log `PROBE: epd_<mode>_ms=<n>`
     for the region, plus a full-screen quality refresh.
  7. **SD card + display coexistence:** mount the SD card with the same pins/approach
     as `src/config_sd.cpp` (reuse that file if it can be compiled into the probe;
     otherwise duplicate minimal code and say so). List the root directory names (names
     only, never file contents; a real config.yaml holds credentials), unmount, then
     draw and refresh a pattern again. Log `PROBE: sd=<ok|nocard|error>` and
     `PROBE: display_after_sd=ok`. The user confirms the redraw on screen.
  8. **Done:** show a summary of the key results on screen and log `PROBE: done`.

Out of scope: Wi-Fi, sleep/power-off, RTC alarm wake (later power task), any app UI.

## Files to touch
- platformio.ini (add `[env:probe]` only)
- probe/main.cpp (new)

## Acceptance criteria
- [ ] `pio run -e m5paper` and `pio test -e native` are unaffected; `pio run -e probe` builds.
- [ ] Flashed probe (via `tools/serial_capture.py --reset --seconds 600 --until 'PROBE: done'`)
      produces every `PROBE:` key listed above, or an explicit `PROBE: <key>=unsupported`.
- [ ] Rocker mapping, touch errors per rotation, EPD timings and SD result are reported
      from a real run with the user following the prompts.
- [ ] After the run, the main firmware is flashed back (`pio run -e m5paper -t upload`).

## Notes
- Serial: `pio device monitor` needs a TTY; use `tools/serial_capture.py`. Its `--reset`
  uses the DTR+RTS pulse that works on this CP2104 board. Each reset re-runs setup.
- Result (run 2026-10-27, 58 s, single pass): see "Hardware facts" in ARCHITECTURE.md.
  Full log was kept in the session scratchpad (not committed; contains SD entry names only).
- Open: whether the side button is separate from the rocker (the log only shows BtnB);
  which physical direction BtnA is in landscape; the full-screen quality refresh time;
  where the 2.6 s in `M5.begin()` goes.
