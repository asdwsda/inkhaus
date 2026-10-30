# 072: Fetch states only for the visible tab

Status: todo
Depends on: 023, 036, 037
UI spec: none
Needs device: yes

## Scope
User (2026-10-30): on a wake or a scheduled refresh, refresh only the widgets of the visible tab.
- The `/api/template` state request lists only the visible tab's entities (plus entities the status bar
  or an open popup needs).
- Switching tabs draws the new tab from the state cache (037) at once, then fetches that tab's states
  over the kept-alive connection (036) and updates changed widgets.
- Actions and their optimistic UI are unchanged.
Out of scope: history graph widgets (071 follows the same rule there).

## Files to touch
- lib/app_logic (session: entity set per request), src/ session/app wiring.

## Acceptance criteria
- [ ] Native test: a session's state request contains only the visible tab's entities.
- [ ] Native test: a tab switch triggers a fetch of the new tab's entities; cached values are shown first.
- [ ] Device: wake, then switch tabs: the new tab shows cached values, then fresh ones within ~1 s
  (debug log shows one request per tab).
- [ ] pio test -e native and pio run pass.

## Notes
- Saving: a smaller template response per wake. Cost: one extra request on each tab switch while awake.
