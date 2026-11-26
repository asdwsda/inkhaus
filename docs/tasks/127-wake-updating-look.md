# 127: Wake: show "updating" until fresh, then one refresh

Status: todo
Depends on: 120, 126 (pending look)
UI spec: docs/ui/wake-updating.md (to write)
Needs device: yes (look + timing; PC part first)

## Scope
User feedback (2026-11-26): after a wake it's unclear when the UI is ready and taps are useful; the wake also
feels slow. Wanted: right after the wake, all state widgets get a cheap "not fresh yet" look (greyed or dashed,
designer proposes, user picks); while updating, a "refresh running" indicator shows; when the fresh states
arrive, everything is redrawn in ONE refresh. Taps during the updating look still work (optimistic, queued).
Out of scope: scheduled (non-interactive) wakes, sleep screen, connection speed itself.

## Files to touch
- lib/ui_state, src/render, src/present, src/app_frame.cpp, test/

## Acceptance criteria
- [ ] Interactive wake: at most one partial for the updating look (fastest if pure B/W) + one refresh when fresh.
- [ ] No per-widget partials while states trickle in; a timeout (e.g. offline) ends the updating look with the
      offline/stale status.
- [ ] Tapped widgets during updating show their pending look and act normally.
- [ ] Native tests for the state machine and the refresh budget; `pio test -e native`, `pio run` pass.
- [ ] Device look check by the user.

## Notes
- Spec written (ui-designer, 2026-11-26): docs/ui/wake-updating.md, mockups docs/ui/experiments/png/wake-updating-*.png
  (check_mockups: 0 problems). Variant A (corner brackets + clock badge, fastest) recommended; B (greyed, quality). User picks.
- Decisions (architect, 2026-11-26): the wake stale-look partial counts toward ghost debt by area (as 120's indicator
  partials); removing the stale look does not count as a changed widget for the >6 GC16 rule.
