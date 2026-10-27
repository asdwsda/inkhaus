# 015: E-ink persistence and refresh-mode experiment

Status: done
Depends on: 013, 014
UI spec: none (diagnostic screens)
Needs device: yes (the user answers prompts with the rocker)

## Why
In task 014 the user saw, after a reset: the title gone (only the counter box was refreshed),
and the counter digits garbled. The design depends on "wake into the frozen screen", so the
cause must be known, not guessed. Two questions:
1. Does the display start-up (M5GFX autodetect: IT8951 reset on GPIO23, BUSY wait, panel init)
   wipe or refresh the panel by itself? If yes, can it be avoided, and how?
2. Which refresh mode gives clean, ghost-free updates of a small region, and what does each cost?

## Scope
A second diagnostic env, `[env:epdtest]` (extends `m5paper`, compiles only `epdtest/`), so the
probe and the app stay untouched. Logs `EPD: <key>=<value>` on serial and drives the user with
short DejaVu40 prompts. User answers: rocker UP = yes, rocker DOWN = no, PUSH = skip (user request:
a push easily tilts the rocker up or down, so yes/no must be distinct directions).
The step state persists in NVS (namespace `epdtest`), so the test survives the resets it causes.

1. **Source reading first (no device):** read M5GFX's M5Paper autodetect and Panel_IT8951 init
   (`.pio/libdeps/m5paper/M5GFX/`). Does init send a clear or a refresh command (e.g. an
   "init" waveform or a full white update), or only reset the controller? List the exact
   calls and lines in the report. Decide whether a display start-up WITHOUT the IT8951 reset
   is possible (e.g. building the Panel_IT8951 + touch config by hand, or a config flag), and
   implement it as variant B.
2. **Persistence test, variant A (stock start-up, as in task 014):** draw a reference screen with a
   full quality refresh: big "PERSIST" text, 4 corner squares, and a 72 px "1". Ask "Do you see
   PERSIST and four squares?" in a small bottom strip, refreshing only that strip. Then restart
   (`ESP.restart()`). After the restart, refresh ONLY a small bottom strip with the question
   "Is PERSIST still visible?". Log `EPD: persist_stock=<yes|no>`.
3. **Persistence test, variant B (start-up without the reset, if step 1 found a way):** the same,
   log `EPD: persist_noreset=<yes|no|unsupported>`. Also check that touch and partial
   refresh still work after this start-up (one tap target, yes/no).
4. **Ghosting / mode test:** four boxes side by side, labelled A–D, each about 200x200 with one
   72 px digit. In each box count 1→6, about 1 s per step, and log the refresh time per update:
   - A: `epd_fastest`, draw over (white fill + digit, one refresh)
   - B: `epd_fast`
   - C: `epd_text`
   - D: `epd_fastest` digit after a white fill refreshed with `epd_quality` (two-step)
   Then ask per box "Is box X readable and clean?" and log `EPD: ghost_<A..D>=<yes|no>` with
   the times as `EPD: mode_<A..D>_ms=<avg>`.
5. **Anti-aliasing:** in `epd_fastest`, draw the same word with the DejaVu (anti-aliased) font and
   with a non-anti-aliased variant (or the text color forced to pure black/white). Ask which is
   cleaner, and log `EPD: aa_fastest=<aa|noaa|same>`.
6. **Done:** log `EPD: done` and show a summary.

## Files to touch
- platformio.ini (add `[env:epdtest]` only)
- epdtest/main.cpp (new)

## Acceptance criteria
- [ ] `pio run -e m5paper`, `pio run -e probe`, `pio run -e epdtest` and `pio test -e native` pass.
- [ ] The report states, with source references, whether and where the stock start-up wipes the panel.
- [ ] A real run with the user produced every `EPD:` key (or `unsupported`/`skipped`).
- [ ] The main firmware is flashed back afterwards.

## Notes
- Use `tools/serial_capture.py` (no TTY). The test resets itself: start a long capture with
  `--reset` once and don't reset again manually.
- User observation (2026-10-27, box test, beyond the yes/no answers):
  - A (`epd_fastest` over the old digit): very little ghosting, but the box background turns
    grey instead of white.
  - B (`epd_fast`): better than C, still ghosting.
  - C (`epd_text`): heavy ghosting in the background.
  - D (white fill with `epd_quality`, then the digit with `epd_fastest`): clean, white background.
  - Guidance given to the user: only D counts as clean (A's grey background is residue).
- User observation, AA test: A (anti-aliased DejaVu) and B (thresholded black/white) look almost
  the same, both clean; the user chose B as "a tiny little bit cleaner". The log shows `noaa`.
- Result: see "Hardware facts" and the 2026-10-27 decisions in ARCHITECTURE.md. Note that
  `ghost_X` holds the answer to "Is box X clean?", so `ghost_D=yes` means D is clean (the
  tester report read it inverted). Source reading: the stock start-up sends no refresh or clear
  command, only the reset pulse, GET_DEV_INFO, SYS_RUN and register setup.
- The task 014 "title gone" observation is not explained by the start-up; it gets rechecked on
  the final task 014 firmware.
