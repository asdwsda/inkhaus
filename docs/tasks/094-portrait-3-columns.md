# 094: Portrait 3-column grid (`portrait_columns: 3`)

Status: in-progress
Depends on: 043 (landscape_columns: 4)
UI spec: docs/ui/layout.md section 2b (to be written), mockups docs/ui/experiment-portrait-3col{,-cover3}.svg
Needs device: yes (visual check)

## Scope
User 2026-11-02: per-tab `portrait_columns: 3` (default 2), analogous to `landscape_columns: 4`. Landscape unchanged.
- Grid: 3 columns x 7 rows, column x 16/189/362, widths 161/161/162, gap 12 (experiment mockup).
- Wide widgets (cover, cover group, climate, vacuum): default 3x1 (existing wide layout, full row); `size: 2x1` allowed with a
  compact layout (F18 name/state, truncation with "…", spec 2b).
- Graph widget: default 3x2 in a 3-column tab; 2x1/2x2 allowed. Sensor/weather 2x1 allowed.
- 1x1 widgets at 161 px: names that don't fit at F24 fall back to F18, then "…"; state text that doesn't fit next to the icon
  is shortened per spec 2b.
- Config parse/validation + docs/CONFIG.md + config.example.yaml comment. Out of scope: 3+ columns in landscape changes.
- Then the user's config.yaml: Home tab gets `portrait_columns: 3` (architect does this after the firmware lands).

## Files to touch
- lib/config (parse `portrait_columns`), lib/ui (ui_layout grid + sizes), src/render (1x1 font fallback, compact cover)
- test/ (layout + config), docs/CONFIG.md, config.example.yaml

## Acceptance criteria
- [ ] Native tests: 3-col portrait grid rects match the spec; the user's Home tab (config.yaml) lays out in 7 rows, no pager;
      cover default 3x1, `size: 2x1` honoured; invalid values (not 2/3) rejected with a config error; landscape unchanged.
- [ ] Rendering matches docs/ui/experiment-portrait-3col-cover3.png (screenshot on device, debug build).
- [ ] 2-column tabs render exactly as before (existing layout tests unchanged).
- [ ] `pio run` and `pio test -e native` pass.

## Notes
