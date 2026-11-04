# 107: Media album art

Status: PC done, device path written (UNVERIFIED, no device yet)
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
- User 2026-11-04: demo "New" tab keeps 2 media tiles (playing with album art, paused); drop the "off" tile so the art version fits.
  Demo art is a built-in sample image (no network in demo mode).
- PC part done (2026-11-04): lib/media_art (art_image: crop/scale/16-grey dither/sample cover; art_fetch: URL plan, token rule, 150 KB cap; art_store: per-URL state machine, popup 200 px buffer, demo serving), `art` config key (+ cache bit 6), media_layout (widget + popup rects of the art spec, art:false keeps the 106 layout), ui_layout sub-region 5 (art, touch only), AppState::artSync / takeArtRequests, src/render/media_widget (art, tiles, popup), demo "New" tab (living room playing with the sample cover, kitchen paused without a picture; off tile dropped), docs (guide, config.example.yaml, device-checks 107). Tests: test/test_media_art.
- Open for the device part: HTTP fetch of `entity_picture` (ArtStore::takeRequests -> planFetch -> download with caps -> JPEG/PNG decode -> Gray8 -> ArtStore::deliver/fail; retryFailed on wake), the GC16 art-rect-only partial refresh (today the generic frame diff in src/present picks the mode), real icons `volume` L for the tiles (primitive glyph used), look check on the demo tab.
- Device path written 2026-11-04 (all UNVERIFIED, build + native tests only): NetCommand::Art / NetEvent::ArtDone on the background lane (yield -> requeue like Forecast), HaWorker::runArt (HaClient::stream_get / stream_url, 150 KB and 6 s caps, token only to HA), src/art_glue (pump one download at a time, M5GFX drawJpg/drawPng into a 16-bit PSRAM sprite with 1/2/4/8 reduction, luma -> squareScale 200 -> ArtStore::deliver; Cancelled -> ArtStore::requeue; failed ones retried per net session), present::setGreyRegions (a changed rect touching ready art = one GC16 with real greys, reason=art). Pure helpers art::imageInfo / planDecode tested. Open: device look, real PSRAM use of the 150 KB std::string body, decode time, real `volume` glyph.
