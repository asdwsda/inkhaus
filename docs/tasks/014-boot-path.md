# 014: Boot path on the device

Status: done
Depends on: 002, 013
UI spec: none (boot screen is temporary)
Needs device: yes (flash + visual check)

## Scope
Make the app's boot correct and measurable, as the base for drawing, Wi-Fi and sleep.
- `src/boot.{h,cpp}`: the one place that brings up hardware.
  - `Serial.begin(115200)` first; every log line is `BOOT: <key>=<value>`.
  - M5 config: `clear_display = false`; turn off what we don't use (IMU, speaker, LEDs,
    external ports, etc., whatever M5Unified 0.2.28 offers for this board); `serial_baudrate`
    stays 0 since we start Serial ourselves.
  - Timestamps (`micros()`) for: before `M5.begin`, after it, after the settings load,
    after the first draw. Log `BOOT: t_<stage>_ms=<n>`.
  - Find where the ~2.6 s inside `M5.begin()` goes (read M5Unified/M5GFX source for the
    M5Paper path, try the config switches, and time `M5.Display` init separately if
    possible). Report the breakdown and what was cut. Target: `M5.begin` well under 1 s,
    if achievable without breaking the display or touch.
- Rotation from settings (`src/settings_nvs`): portrait → rotation 0, landscape → rotation 1.
- `src/main.cpp`: use boot; draw a temporary screen with `fonts::DejaVu40`/`DejaVu24`:
  title, version (`FW_VERSION` bumped to 0.2.0), and a boot counter (NVS key `boots`
  in its own namespace `diag`) in a small box.
  - The first boot ever (counter 0, or the panel content unknown) does one full
    quality refresh.
  - Later boots redraw ONLY the counter box with `epd_fastest` (no full clear,
    no full refresh), so a reset shows no flash.
- Log reset reason and battery mV on every boot.

Out of scope: config/SD loading, Wi-Fi, sleep, real UI.

## Round 2 (after task 015)
Apply the 2026-10-27 decisions from task 015 (see ARCHITECTURE.md):
- **Start-up without the IT8951 reset**, as in epdtest variant B (69 ms instead of 1070 ms),
  with a fallback to the stock start-up if the controller doesn't answer (GET_DEV_INFO fails,
  or the panel size is wrong). Keep M5Unified for power, buttons, RTC and touch if possible,
  e.g. by giving it our display device or by beginning it without its display autodetect
  (find the supported way in the M5Unified/M5GFX sources). If M5Unified can't be kept without
  the reset, say so and propose the minimal alternative. Don't silently drop features.
- **Screen tracking:** NVS records what was last drawn (a screen id + content hash). If the record
  is missing or doesn't match, do one full quality redraw. User report: after another firmware
  ran, our counter-only update drew faintly over that firmware's screen.
- **Refresh strategy:** the counter update uses `epd_fastest` with thresholded black/white text.
  Every region drawn that way is tracked, and before setup finishes (stand-in for "before sleep")
  each tracked region gets one clean pass: white fill with `epd_quality`, then the content.
  (Order per task 015 box D.)
- Log `BOOT: start=noreset|stock`, `BOOT: display_start_ms`, `BOOT: draw=full|partial` and the
  reason for a full draw.

## Round 3 (after the user's 3-reset check)
User check, 3 hardware resets (boots 29–31): title stays, counter readable, box clean. BUT on
each cold boot there is one full-screen flash before the two box changes, and the background
outside the box carries a light grey veil ("a little bit ghosty").
Diagnosis from the logs: after `rerender=yes`, the counter refresh in `epd_fastest` waits
549 ms, while the cleanup's `epd_fastest` box refresh waits 136 ms. So the counter refresh
covers the whole screen (the pending refresh range from the re-render is not limited to the
box), and a full-screen fastest refresh leaves the grey veil (task 015, box A).
- Fix: after re-rendering into controller memory, the next refresh must cover only the counter
  box. Find the real mechanism in Panel_IT8951 (how `display(x,y,w,h)` builds its range and
  how the re-render's pixels are flushed), and make the re-render write to memory without
  adding to the refresh range. Prove it: the counter refresh `wait_ms` on a cold boot must be in
  the same ~135 ms range as the cleanup's fastest refresh.
- Full redraws over an unknown/foreign screen use the panel's strongest clearing waveform
  (the IT8951 INIT/GC16 full flash, whatever Panel_IT8951 exposes), so earlier screens leave no
  residue. Log `BOOT: full_mode=<mode>`.
- Not part of the fix, but report it: is there a cheap way to clean an existing grey veil
  without a full flash? (Probably not; then the full draw above is the answer.)

## Files to touch
- src/boot.h, src/boot.cpp (new)
- src/main.cpp
- include/version.h

## Acceptance criteria
- [ ] `pio run -e m5paper`, `pio run -e probe` and `pio test -e native` pass.
- [ ] On device (`tools/serial_capture.py --reset --seconds 15`): `BOOT:` lines appear with
      timings, reset reason and battery mV.
- [ ] On device: after a reset, only the counter box changes and there is no full-screen
      flash (user confirms); the counter increments across resets.
- [ ] On device: after a reset the title and version are STILL visible (the whole screen is
      re-rendered into the controller's memory without a refresh, then only the counter box
      is refreshed). User report on the first attempt: counter at 2, title gone.
- [ ] Report: the `M5.begin` time breakdown before and after, and which options were changed.
- [ ] Round 2: `BOOT: start=noreset` and a display start-up under 200 ms on a normal reset;
      the fallback path is exercised once (e.g. by a forced compile-time flag) and works.
- [ ] Round 2: after flashing over another firmware's screen, the first boot does a full redraw
      (`draw=full reason=unknown_screen`); the following resets draw partially.
- [ ] Round 3: on a cold boot the counter refresh waits ~135 ms (not ~550 ms), and the user
      sees no full-screen flash, only the box changing.
- [ ] Round 3: after a full redraw over a foreign screen the background is clean white (user).
- [ ] Round 2: across 3 consecutive resets the user confirms the title stays, the counter is
      readable each time, the background is white after the cleanup, and there is no full flash.

## Notes
- Each `--reset` re-runs setup. Use as few resets as needed; the user is watching.
- Result (3 rounds): no-reset display start-up with stock fallback, M5Unified kept (`setPanel` +
  board id), screen tracking (`lib/screen_track`: record + panel signature), auto-display off,
  fastest-then-clean box updates, GC16 full redraws. User-verified on boots 29–31 and 53: title
  kept, counter readable, box clean, no full flash, clean background after a full redraw.
- Accepted: cold display start-up ~1.06 s (the IT8951 power-cycles on a hardware reset); the
  first box refresh after a cold start ~430 ms.
- Moved to task 016: the warm-boot hang in `panelSignature()`, bounded waits, the watchdog,
  and invalidating the record before a full draw.
