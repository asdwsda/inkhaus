# 017: Render the real UI on the device (demo data, no network)

Status: done
Depends on: 011, 014, 016
UI spec: docs/ui/STYLE.md, docs/ui/layout.md, docs/ui/widgets.md (revision 4); reference images docs/ui/png/
Needs device: yes (user looks at the screen; screenshots over serial)

## Scope
First version of the real app screen: config-driven tabs and widgets, drawn as specified,
navigable, with local fake state. No Wi-Fi, no HA, no sleep.
- **Config source:** load `/config.yaml` from the SD card via `src/config_sd` + `lib/config`.
  If it is missing, use a **demo config** compiled in from `config.example.yaml` (embedded at
  build time; it contains only fake values), and show "Demo" in the status bar. A config with
  errors shows a simple error screen listing the first error with its path and line (not the
  Settings screen; that's task 007).
- **Demo states:** a `model::StateStore` filled with plausible values matching the mockups
  (`docs/ui/flow/` frames are a good reference), so every widget type shows a realistic state.
- **Rendering:** `src/render/` (or similar): draw into an `M5Canvas` (8-bit grey, PSRAM) at the
  current orientation, using `lib/ui` layout rects and `subRegions` exactly. One drawing
  function per widget type, following docs/ui/widgets.md (fonts, center lines/baselines, icons,
  states incl. pending/unavailable). Status bar (no Wi-Fi yet: show the "Demo" label instead of
  the connection icon; the battery from `M5.Power`), tab bar, pager.
- **Push and diff:** push the canvas to the panel; compute changed rectangles by diffing against
  the previous frame; use the refresh strategy from ARCHITECTURE.md (fastest for feedback with
  `DirtyTracker`, a clean-up pass after 2 s without input, GC16 for tab/page changes and full
  redraws). The cold-boot path from task 014 becomes "render the frame, push it to controller
  memory without a refresh, refresh only what differs from the last known frame".
- **Input:** touch via `M5.Touch` + `lib/ui` `hitTest`; rocker via `M5.BtnA/B/C` + `focusHandle`
  (draw the focus ring per spec). Tap/push on a widget changes its fake state locally: toggles
  flip; cover up/down/stop changes position/state; climate −/+ changes the target by the step.
  Show the pending state for ~600 ms, then the confirmed one (simulating HA), so the pending
  visuals get exercised.
- **Screenshot over serial:** sending `shot\n` on serial dumps the current canvas as a binary
  PGM framed by `SHOT-BEGIN <w> <h>` / `SHOT-END`. Add `tools/screenshot.py` (pyserial) that
  sends the command and writes a PNG (via Pillow if available, else PGM).
- Orientation from settings (portrait default); include a temporary way to flip it for testing
  (e.g. rocker long push + push within 1 s is NOT needed: a `-DDEMO_LANDSCAPE` build flag is enough).

Out of scope: Wi-Fi, HA, sleep, Settings screen, real battery icon tuning.

## Files to touch
- src/main.cpp, src/render/* (new), src/input.* (new), src/app_state.* (new, if needed)
- platformio.ini (only to embed config.example.yaml, e.g. `board_build.embed_txtfiles`)
- tools/screenshot.py (new)
- lib/* only for small, tested additions (e.g. a frame-diff helper in lib/screen_track)

## Acceptance criteria
- [ ] Builds (m5paper/probe/epdtest) and `pio test -e native` pass; any new lib logic is unit-tested.
- [ ] Screenshot of the demo "Living room" tab in portrait and landscape compared with
      docs/ui/png/tab-portrait.png and tab-landscape.png: widget frames at the spec rects
      (pixel-exact), and text/icons within a few px of the mockup. Report a diff image and
      the mismatches.
- [ ] On device (user): tab switch by tap and by rocker; a lamp toggle shows pending then on;
      cover and climate buttons change state; no full-screen flash except on tab/page changes;
      the background stays clean after the clean-up pass.
- [ ] Cold boot (hardware reset) shows the same screen without a full flash (task 014 behaviour kept).
- [ ] Missing config.yaml → demo with a "Demo" label; a broken config → error screen with path + line
      (tested with a deliberately broken file on the SD card, written by the user or via a
      test-only build flag; never read or overwrite a real config.yaml).

## Notes
- The user's SD card currently holds no config.yaml.
- Carried over from task 016 (small, do them while in src/main.cpp and src/boot.cpp):
  log the `esp_task_wdt_init` return code (and show that the 15 s timeout is in effect);
  if `invalidateRecord()` fails, don't leave a valid record behind a full draw; keep BUSY's
  pull-up in `waitBusyHigh`; handle a failed `waitDisplay` in `refresh()` (log + recovery).

### Progress (firmware-dev, session 2): implementation complete, verified on the device
Everything in the scope is done. Test-only build flags (via `PLATFORMIO_BUILD_FLAGS`): `DEMO_LANDSCAPE`,
`TEST_CONFIG_BROKEN` (injected broken YAML), `TEST_CONFIG_MOCKUP` (the mockup's tab, mock status label,
frozen pending); `BOOT_TEST_RESTART`/`BOOT_TEST_HANG` from 016 still work. Serial test commands: `shot`,
`shotz`, `tap X Y`, `key up|down|push|long`, `pend MS` (tools/screenshot.py `--send`).
Layout: lib/ui_state (pure: views, fake actions, demo states, AppState), src/render (draw, widgets,
screen), src/present (diff, refresh modes, record), src/input, src/shot, src/app_config, src/main.
Open: user check of tab/rocker/pending/clean-up on the device; ghost-debt limits (12 / 150 %) untuned;
no Settings (LONG_PUSH in TAB mode and taps on the battery/refresh areas do nothing yet).
- User hands-on check (2026-10-28), round 1:
  - Bug: on both cover widgets, right after a tap, the state text shows old and new text on top
    of each other; the clean-up pass (≈2 s later) makes it clean. Screenshots (canvas) are correct,
    so the fault is in push/refresh (controller memory vs panel).
  - The rocker skipping the Window (binary_sensor) widget is by spec (read-only widgets are
    skipped); explained to the user.
  - Architect review of screenshots: frames pixel-exact; text up to 8 px wider (bitmap DejaVu);
    status bar "100%" touches the battery icon; window shows "On" (demo state has no
    device_class); clean-up turns DIM grey text black (by the task 015 recipe).
- Fix round 1 (firmware-dev, 2026-10-28):
  - **Root cause of the double text:** `pushRegion` pushed the thresholded 0/255 frame with
    `pushGrayscaleImage(..., (uint32_t)TFT_WHITE, (uint32_t)TFT_BLACK)`. In this M5GFX `TFT_WHITE` is the
    RGB565 value 0xFFFF; cast to uint32_t it is read as RGB888 = (0,255,255), luma 765 of 1020. The IT8951
    fast write dithers against a Bayer table (+-30*16 around 512), so that "white" became black on 1 pixel
    of 4 (every other pixel on every other row). Memory under every freshly pushed white area held a 25 %
    black dither; after the confirmed frame the old text (never overwritten by white) plus the dots showed.
    Fix: `0xFFFFFFu, 0x000000u`. Cost: none; no two-step needed.
  - **Evidence (new debug tooling):** serial `mem x y w h` (IT8951 image memory of a rect, RLE; always
    followed by `endBurstRead`, and preceded by one: after a refresh the first memory read otherwise stalls
    BUSY for minutes), `shotp` (the presenter's `prev` frame), `cleanup 0|1`; `tools/screenshot.py --mem X Y W H
    [--steps --send ...]` fetches mem + cur + prev per step. Before the fix, shutter state rect (16,90,280,70)
    after the confirmed frame: memory differed from thresholded `prev`/`cur` in 554 px (all "memory black,
    frame white"); a plain white write over (96,128,104,23) left 624 px black in 12 rows. After the fix:
    0 differing px before the tap, after the pending frame, after the confirmed frame and after the clean-up.
  - Small fixes: battery icon moves left so a 4 px gap to "100%" is kept (src/render/screen.cpp);
    window sensor with `device_class: window` reads Open/Closed (lib/ui_state, demo state, unit test);
    clean-up content pass uses `epd_quality` (grey image, not thresholded) for regions with grey pixels
    (about 440 ms per region instead of about 130 ms, so +300 ms per region; typical regions all contain grey);
    probe/epdtest erase the app's `diag` record at startup.
- User hands-on check, round 2 (2026-10-28, clean-up disabled via serial):
  - Shutter state changes cleanly (colour-constant fix confirmed). Window "Open", battery % OK.
  - Whole screen had a faint striped grey pattern (residue of the dither bug); a tab change cleared it.
  - Bug: after switching tabs back and forth, the first 1–2 taps on a shutter show old + new text;
    from the third tap on it's clean. The log shows a GC16 full refresh on the tab change, then
    `epd_fastest` on the state-text rect → fastest can't drive GC16 greys to white.
  - Grey state text drawn black is fine for the user.
  - Decisions (ARCHITECTURE.md 2026-10-28): fastest only over black/white content, else one
    quality refresh; no per-tap clean-up; dynamic text pure black.

- Fix round 3 (firmware-dev, 2026-10-28), per the three 2026-10-28 decisions:
  - src/present: a third frame `g_panel` = exactly what was pushed (thresholded or not); `g_prev` stays the last
    composed frame (diff reference); `shotp` now dumps `g_panel`. Per diff rect: panel has any pixel not 0/255 ->
    one `epd_quality` refresh, else thresholded push + `epd_fastest`. Log: `PRESENT: rect=x,y,w,h mode=fastest|quality
    reason=panel_bw|panel_has_grey|boot_unknown_panel`. Boot-diff rects always use quality (panel content known only by hash).
  - Deviation, for the architect: the quality refresh pushes the THRESHOLDED image (`kQualityPushesGrey=false`), not the
    unthresholded one. Icons and glyph edges are anti-aliased in the canvas, so an unthresholded push would leave greys in the panel and
    every later update of that rect would be quality again, never "once". GC16 shows 0/255 exactly. Flip the constant to
    push greys.
  - Per-tap clean-up removed (tracker unused). Ghost debt = fastest refreshes since the last full refresh: 30 frames or
    300 % screen area (untuned; quality refreshes add none) -> one GC16 full refresh in `idle()` after 2 s quiet.
    `prepareSleep()` (not wired yet, no sleep in 017) does a full refresh at half those limits. `cleanup` serial command is a no-op.
  - src/render/widgets.cpp, dynamic text now INK (was DIM / OFFGREY): cover state; toggle state text (PAPER on ink fill
    as before); climate temp, target (also when unavailable); sensor value; weather temp, condition (wide and 1x1);
    vacuum status; sensor/weather/climate "unavailable" data colour. Unchanged (grey): titles when unavailable, frames,
    dashed boxes, badges, warn icons, error-screen labels, tab-chrome decoration.
  - Proof (scratchpad/memtest.py, log memtest.log): tab and back, 4 shutter taps; after each step IT8951 memory vs
    `g_panel` for every refreshed rect (full refreshes in 60 px strips): 48 checks, 0 differing px (tolerance 40/255).
    Modes: after the tab-back GC16, tap 1 and 2 gave quality for the button/icon rects (they hold anti-aliased greys),
    fastest for the state-text rects (pure black text, already bw); from tap 3 on everything was fastest (125-135 ms).
    Totals in the run: 5 quality, 15 fastest rects. Device left with the normal portrait demo flashed.
- Result (3 rounds + review fixes): config-driven tabs and widgets rendered into a PSRAM canvas
  from lib/ui rects; frame diff → per-rect fastest (black/white content) or quality (grey content);
  GC16 on tab/page changes and ghost debt; touch + rocker with focus ring; local fake actions with
  pending → confirmed; demo config embedded; error screen; serial screenshot/test commands.
  Root cause of the overlapping text: a 16-bit colour constant read as 24-bit (25 % dither).
  User-verified on the device (round 3); 48 controller-memory checks with 0 differing pixels;
  code review fixes (forceFull, allocation failure, endBurstRead after every refresh, climate step,
  number parsing). 328 native tests.
- Open for later: ghost-debt limits untuned (30 frames / 300 %); `prepareSleep()` gets wired in
  with the sleep task; serial test commands are compiled in (decide on a release guard later);
  UI spec update for "dynamic text is black" (ui-designer).
