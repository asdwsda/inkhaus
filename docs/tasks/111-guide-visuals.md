# 111: Guide visuals: widget fragments, annotated screens, diagrams

Status: todo
Depends on: 110 (page layout), accepted mockups
UI spec: docs/ui/guide-visuals.md (ui-designer, to write)
Needs device: no

## Scope
ui-designer plans and draws the visual aids; a tool renders them to cropped PNGs for the guide.
- Widget gallery fragments: every widget type x every size it supports x its main states (on/off/unavailable/attention),
  plus option comparisons (e.g. `size: 2x1` vs default, `name:` / `icon:` overrides, `art: false`, history line vs bar),
  each next to the exact config.yaml snippet that produces it.
- Annotated screens: tab screen anatomy (status bar, tabs, battery, "updated" = tap to refresh, sleep pill), popup anatomy,
  Settings overview + each page, sleep screen variants, portrait vs landscape and `portrait_columns` 2/3/4.
- Device drawing (vector, not a photo): front + side with rocker (up/down/press, long-press = Settings), reset button,
  USB-C, SD slot, power; a gestures cheat sheet.
- Diagrams: install flow (web flasher -> SD card files -> first boot -> HA token), SD card file tree, sleep modes vs
  battery-use level table, how the device talks to HA (wake, fetch, sleep).
- Troubleshooting screens: offline, HA unreachable, auth error, entity unavailable, low battery, each with the fix.
- Pipeline: fragment SVGs in `docs/guide/img/src/`, `tools/render_guide_images.sh` (same Inkscape + DejaVu setup as
  render_mockups.sh) -> optimized PNGs in `docs/guide/img/`; check_mockups-style lint; images reuse the accepted mockups'
  geometry so they match the device.
- Later (device): replace gallery fragments by real `shot` screenshots where they differ (debug build serial command).

## Files to touch
- docs/guide/, site/, tools/ (110, 111); docs/ui/ (111, ui-designer)

## Acceptance criteria
- [ ] Spec docs/ui/guide-visuals.md listing every image (file, what it shows, caption, which guide section).
- [ ] All listed images rendered + checked (0 problems), each <= 60 KB, alt text drafted.
- [ ] Rerunning the render tool reproduces the PNGs byte-identically or near (no manual edits).

## Notes
- User 2026-11-03: docs must be as user friendly as possible, part of the website, with widget screenshots and options.
- User 2026-11-03: style of batch 1 OK, continue; real device `shot` screenshots replace drawn fragments after device checks; shrink showcase GIF when room is needed; device drawing from real product photos (M5Stack docs).
