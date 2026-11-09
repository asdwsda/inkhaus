# 105: Calendar widget

Status: shipped v1.4.0, device check pending
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

## Notes
- PC part done: lib/calendar (cal_events: request/parser/store/view/layout), src/calendar_glue (fetch rule, demo data), src/render/calendar_widget, test/test_calendar; net via a plain background command (Type::Calendar, no net lanes on this base); demo.yaml got a 5th tab "Calendar" (test_ui_state updated: 5 tabs, 2 unavailable). No new config keys (entity, name, size only); docs/CONFIG.md is now a stub, so the guide reference lists `calendar` as a domain/type. docs/guide/04-widgets.md still lists the calendar under "Coming soon" (not touched).
- User 2026-11-03: no such entity in the user's HA. Demo mode must show this widget with sample data (all states), so the device look check works without a real entity.
- 2026-11-03: the guide gallery picture for this widget (docs/guide/img/gallery-*) embeds a config snippet from docs/ui/widgets-new.md; implement those keys exactly (or fix the spec + picture), so the guide snippet parses.
