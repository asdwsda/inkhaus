# 108: Alarm panel, lock and garage widgets + confirm dialog

Status: shipped v1.4.0, demo ok; real entities pending
Depends on: 045
UI spec: docs/ui/widgets-new.md §0, §5, §6, §7 (+ mockups docs/ui/png/widgets-new-*.png)
Needs device: yes (look + a real entity; PC part first)

## Scope
Alarm 1x1 + keypad popup (numeric codes; Home/Away/Night; alarm_control_panel.alarm_arm_* / alarm_disarm with code), lock and garage cover 1x1 with the shared confirm dialog (lock: confirm before unlock; garage: both ways). Codes never logged.
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
- PC part done (firmware-dev): config types alarm/lock/garage + `confirm`, lib/ui_state secure_view / confirm_dialog / alarm_popup / app_state_secure, service calls + secure_intent, src/render/secure_widget, demo tabs Alarm + Doors, tests test_secure_widgets, guide 04/09, docs/CONFIG.md, device-checks 108. Open: `cover` with device_class garage is not auto-detected (`type: garage` needed; guide picture fixed); spec's keypad code-box-only fastest refresh not special-cased; success check after the first confirmation fetch, not 2; code never logged (session log prints service + ids only); portrait popups unmocked.
