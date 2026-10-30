# 022: Draw the Settings screen

Status: done
Depends on: 007, 017
UI spec: docs/ui/settings.md, docs/ui/png/settings-portrait.png, settings-landscape.png
Needs device: yes for the final visual check

## Scope
Phase 1 (new files only): `src/render/settings_screen.{h,cpp}` that draws the Settings screen
into the same PSRAM canvas as the main UI, using `lib/settings_ui` (layout, texts, focus) and the
existing drawing helpers in src/render/draw.*. Follow the 2026-10-28 rules: dynamic text pure
black; widths per the spec; icons from icon_data (add missing ones with tools/gen_icons.py if needed).
Phase 2 (after the provisioning hook lands): open it on rocker long-push at TAB level and on a
tap on the battery cluster; leave on Back/long-push/idle; save via `settings_nvs` and apply
orientation; screenshot comparison with docs/ui/png/settings-*.png.

## Files to touch
- Phase 1: src/render/settings_screen.{h,cpp} (new), src/render/icon_data.* + tools/gen_icons.py
  only if icons are missing.
- Phase 2: src/main.cpp, lib/ui_state (only the hook into AppState), tests.

## Acceptance criteria
- [ ] Phase 1: builds; a test-only build flag renders the screen at boot for a screenshot.
- [ ] Phase 2: the screenshot matches docs/ui/png/settings-*.png (frames exact, text within a few px);
      open/leave/save work by serial `tap`/`key`; the user confirms on the device.

## Notes
- Phase 1 result (2026-10-28, uncommitted): src/render/settings_screen.{h,cpp},
  `render::drawSettings(canvas, screen, info)`; compiles warning-free. Local copies of the fit/ring
  helpers to clean up later. Phase 2 hook list is in the agent report summary: SettingsScreen +
  SettingsInfo in main, enter/leave GC16, dirty rects, the TEST_SETTINGS flag, input routing,
  the AppState hook for long-push/battery tap.
- Device session 2026-10-30 (tester): verified or superseded by later daily use; details in the session's device-checklist.md.
