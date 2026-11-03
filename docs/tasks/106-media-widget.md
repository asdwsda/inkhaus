# 106: Media player widget (without album art)

Status: in-progress
Depends on: 045
UI spec: docs/ui/widgets-new.md §0, §4 (+ mockups docs/ui/png/widgets-new-*.png)
Needs device: yes (look + a real entity; PC part first)

## Scope
Wide widget + popup: play/pause, prev/next, volume on the rocker in the popup, title/artist; services media_player.media_play_pause / next_track / previous_track / volume_set. Leave a clear hook for the art rect (task 107).
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
