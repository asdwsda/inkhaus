# 118: Tap opens history on read-only widgets

Status: todo
Depends on: 046 (history popup)
UI spec: docs/ui/history.md (open rule changes as below)
Needs device: no (PC part; device look later)

## Scope
User 2026-11-04: on read-only widgets (sensor, binary sensor, and any other widget whose tap does nothing today, e.g.
weather/energy where history applies), a short tap opens the history popup. Long press keeps opening it on every widget.
Actionable widgets (toggle, script, cover, media, ...) keep tap = action. Rocker: PUSH on a focused read-only widget opens it
too (if such a widget is focusable). `history: false` / `attribute:` widgets: tap still does nothing. Graph widgets unchanged
(nothing on tap). Pending-tap feedback must not show for this (no network call to HA services).
Out of scope: new popup content.

## Files to touch
- lib/ui_state (tap dispatch), test/, docs/ui/history.md (one line: the open rule), docs/guide (widgets/gestures text)

## Acceptance criteria
- [ ] Native tests: tap on sensor/binary opens the popup; tap on a toggle still toggles; long press unchanged; `history: false` tap = nothing.
- [ ] `pio test -e native`, `pio run` (all envs), `python3 site/build.py && python3 site/check.py` pass.
- [ ] docs/device-checks.md line.
