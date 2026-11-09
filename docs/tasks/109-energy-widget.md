# 109: Energy widget

Status: shipped v1.4.0, device check pending
Depends on: 082
UI spec: docs/ui/widgets-new.md §0, §8 (+ mockups docs/ui/png/widgets-new-*.png)
Needs device: yes (look + a real entity; PC part first)

## Scope
2x1 widget: today's usage and solar as bars, history popup with the usage sensor only, reusing the history-bar rendering and the statistics WS fetch (task 082).
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
- User 2026-11-03: has energy sensors (entity ids to be given); this one gets a real-entity device check.
- User 2026-11-03: energy entity e.g. `sensor.shellypro3em_3ce90e703834_total_active_energy` (Shelly Pro 3EM total).
- 2026-11-03: the guide gallery picture for this widget (docs/guide/img/gallery-*) embeds a config snippet from docs/ui/widgets-new.md; implement those keys exactly (or fix the spec + picture), so the guide snippet parses.
- 2026-11-03 PC part done. Today's value = statistics `change`, period day, one bucket from local midnight (graph entries keyed widget index and index + 1000 for solar, reusing GraphWidgets/fetch/cache); works for total_increasing totals. Config: `type: energy`, `usage`, `solar` (either), `entity` is set to usage (else solar); config cache format 9. Popup = history popup of `usage` (bar, hour, change, hours 24). Not rocker-focusable (spec section 0 over section 8). Single configured row is drawn centred (section 8 first paragraph). Open: HA may omit the running hour in day statistics (lag <= 1 h): see docs/device-checks.md 109.
