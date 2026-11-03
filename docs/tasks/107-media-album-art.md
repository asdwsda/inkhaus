# 107: Media album art

Status: todo
Depends on: 106
UI spec: docs/ui/widgets-new-media-art.md (+ mockups docs/ui/png/widgets-new-media-art-*.png)
Needs device: yes (look + a real entity; PC part first)

## Scope
Album art per docs/ui/widgets-new-media-art.md: fetch `entity_picture` (HA-relative URL, token auth) once per track at 200x200, decode JPEG/PNG (M5GFX/TJpgDec) into PSRAM, scale to 80x80 for the widget, 16-grey dither, GC16 partial of the art rect only, redraw only when the URL changes; per-widget `art: false`. Download size cap (e.g. 150 KB) and time cap inside the session caps; failure -> fallback tile. Architect decision: this is the only grey-image path; keep it isolated (own module).
Follow the shared rules (§0) and grid sizes (§9) in docs/ui/widgets-new.md, existing widget/popup code patterns.
Out of scope: other new widgets.

## Files to touch
- lib/config (widget type + options), lib/ (widget logic/layout), src/render (drawing), test/

## Acceptance criteria
- [ ] Config parsing + widget logic (state mapping, texts, fallbacks: unavailable/loading) covered by native tests.
- [ ] Layout rects match the spec numbers (tests where the layout lives in lib/).
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] docs/CONFIG.md + config.example.yaml document the widget; docs/device-checks.md line; UNVERIFIED tags.

## Notes
- User 2026-11-03: no such entity in the user's HA. Demo mode must show this widget with sample data (all states), so the device look check works without a real entity.
- 2026-11-03: the guide gallery picture for this widget (docs/guide/img/gallery-*) embeds a config snippet from docs/ui/widgets-new.md; implement those keys exactly (or fix the spec + picture), so the guide snippet parses.
