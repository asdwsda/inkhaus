# 103: Notifications widget

Status: todo
Depends on: 101
UI spec: docs/ui/widgets-new.md §0, §1 (+ mockups docs/ui/png/widgets-new-*.png)
Needs device: yes (look + a real entity; PC part first)

## Scope
1x1 widget + list popup with dismiss. Reuse task 101's `persistent_notification/get` WS call (one per session, only when a notifications widget or the sleep-screen block is configured, after graph fetches). Dismiss: WS `persistent_notification/dismiss` (or service `persistent_notification.dismiss`). Attention fill (§0).
Follow the shared rules (§0) and grid sizes (§9) in docs/ui/widgets-new.md, existing widget/popup code patterns.
Out of scope: other new widgets.

## Files to touch
- lib/config (widget type + options), lib/ (widget logic/layout), src/render (drawing), test/

## Acceptance criteria
- [ ] Config parsing + widget logic (state mapping, texts, fallbacks: unavailable/loading) covered by native tests.
- [ ] Layout rects match the spec numbers (tests where the layout lives in lib/).
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] docs/CONFIG.md + config.example.yaml document the widget; docs/device-checks.md line; UNVERIFIED tags.
