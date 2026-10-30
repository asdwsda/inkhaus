# 071: History graph widget

Status: in-progress
Depends on: 049, 052, 053, 055
UI spec: docs/ui/history-widget.md (approved 2026-10-30)
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
Out of scope: several entities in one graph, new graph types; other widgets keep fetching all tabs (user, 2026-10-30).

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
- Reference (dropped 072): branch `ref/072-visible-tab` has `setVisible`/tab-fetch session code and tests that the graph fetch may reuse.
- Scheduled refresh (user, 2026-10-30): update all graph widgets if cheap, else only the visible tab. Rule: measure on the device (debug NETT/timing); if all graphs together add < 0.5 s awake time per scheduled refresh, fetch all; otherwise visible tab only. Estimate from 046: 1-2 s per graph fetch on a warm connection, so expect "visible only".
- Long press: does nothing, same as tap (user, 2026-10-30).
- PC part (branch a60463c, 2026-10-30), accepted deviations:
  - Landscape max height is 3 rows (the pager row).
  - The "missing entity" look is not done: the store can't tell missing from unavailable, so it shows `-`.
  - The cached marker shows only after a failed fetch.
  - Redraws go through the frame diff; check epd_quality greys on the device.
  - Input is ignored during the scheduled-refresh hold (cap 20 s).
  - demo.yaml gets a "Graphs" tab.
  - Scheduled rule constant: `uistate::kGraphFetchAllTabs` (graph_widgets.h). Device logs: `APP: graph_fetch ms=` and `APP: graph_batch`. Also measure `APP: graph_cache ms=`.
- Review (tester, 2026-10-30): pass, no blockers. Device follow-ups:
  - Check `mem` with many graphs.
  - `graphsInit` reads the SD cache under the 15 s watchdog without `boot::feed()`, and before the RTC clock is loaded, so cached data is classed stale. Check `graph_cache ms`.
  - The popup and widget share the per-entity cache key.
