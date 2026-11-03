# 109: Energy widget

Status: todo
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
