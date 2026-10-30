# 071: History graph widget

Status: todo
Depends on: 049, 052, 053, 055
UI spec: docs/ui/history-widget.md (to write; based on docs/ui/history.md)
Needs device: yes

## Scope
A dashboard widget that shows an entity's history graph directly on the tab, drawn like the long-press
history popup (same renderer, graph types, config options and data sources).
- Config: a widget key selects graph display, reusing the `history` options (hours, type, statistic,
  period, min/max, source). Key: `display: graph`.
- Size: wide widget, default 2x2 (full width in portrait); smaller sizes clamp as other widgets.
- Data: fetched on every wake and every scheduled refresh (the existing refresh interval), only for
  graph widgets on the visible tab; switching to a tab fetches its graphs (user, 2026-10-30). The last graph data is cached so the first frame after a wake
  shows it before the fetch finishes.
- Tap: does nothing (no popup, no focus action).
- Shows the current value in the widget header; empty / loading / error states as in the popup.
Out of scope: several entities in one graph, new graph types.

## Files to touch
- lib/config (new key, validation), lib/ui (layout size rules), lib/history (cache reuse),
  src/render (widget drawing), src/ session code (fetch scheduling); docs/CONFIG.md.

## Acceptance criteria
- [ ] Spec with rendered mockups (portrait and landscape) in docs/ui/history-widget.md.
- [ ] Config parser accepts the new key; native tests cover valid, invalid and clamped sizes.
- [ ] Native test: a session requests history only for graph widgets on the visible tab; a tab switch
  requests the new tab's graphs.
- [ ] Demo mode shows a graph widget with the demo history data (068).
- [ ] Device: a graph widget renders on a tab, survives deep sleep (the image stays), updates on a wake
  and on a scheduled refresh; a tap does nothing.
- [ ] docs/CONFIG.md documents the key.

## Notes
- Decisions (user, 2026-10-30): `display: graph`; default 2x2; tap does nothing; refresh on every wake
  and on the scheduled refresh.
- Power: each graph widget adds one history request per session. Fetch graphs only for the visible tab
  and after the state fetch, so taps and states are not delayed.
