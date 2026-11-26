# 127: Wake: show "updating" until fresh, then one refresh

Status: PC part done (firmware-dev 2026-11-26); device look UNVERIFIED, see docs/device-checks.md
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
- User 2026-11-26: variant B (greyed). Taps during updating go through as normal (shown at once, sent when connected).
  Architect: a tapped widget during updating still gets the fastest partial (126 D2), over the greys (ghosting accepted);
  the greyed wake look is one quality partial of the union rect.
- Implemented (firmware-dev): lib/app_logic/src/wake_updating.{h,cpp} (state machine, budget, grey map, end-full rule), status label `updating, taps ok` + refresh icon replace the silent `ready` (wall_time.cpp, screen.cpp), grey pass + clock badge in src/render/widgets.cpp (`StatusInfo::stale`), union refresh `present::unionNextShow()` (bootFrame and show()), wiring in app_frame.cpp/main.cpp/app_loop.cpp, tests test/test_wake_updating. Greys reuse the album-art grey push path (touchesGrey), so they are real greys in a quality refresh.
