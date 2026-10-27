# 017: Render the real UI on the device (demo data, no network)

Status: todo
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
