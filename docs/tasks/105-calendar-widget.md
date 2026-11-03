# 105: Calendar widget

Status: todo
Depends on: 045
UI spec: docs/ui/widgets-new.md §0, §3 (+ mockups docs/ui/png/widgets-new-*.png)
Needs device: yes (look + a real entity; PC part first)

## Scope
2x1/wide widget with the next events. Next event from the entity attributes (message, start_time, end_time, all_day; already in the fetch since task 101); more than one event via `calendar.get_events` (REST service with return_response), bounded window (e.g. 7 days, max N events), only when on the shown tab.
Follow the shared rules (§0) and grid sizes (§9) in docs/ui/widgets-new.md, existing widget/popup code patterns.
Out of scope: other new widgets.

## Files to touch
- lib/config (widget type + options), lib/ (widget logic/layout), src/render (drawing), test/

## Acceptance criteria
- [ ] Config parsing + widget logic (state mapping, texts, fallbacks: unavailable/loading) covered by native tests.
- [ ] Layout rects match the spec numbers (tests where the layout lives in lib/).
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] docs/CONFIG.md + config.example.yaml document the widget; docs/device-checks.md line; UNVERIFIED tags.
