# 126: Instant tap feedback, rapid taps on several widgets

Status: PC part done (device check pending)
Depends on: 116, 120
UI spec: docs/ui/STYLE.md (Pending row); may change after the investigation
Needs device: yes (feel + timing; PC part first)

## Scope
User feedback (2026-11-26, 1.4.x on the device):
1. The dashed pending outline after a tap is never visible, however long the action takes.
2. Tapping several lights one after another doesn't work: each tap waits until the previous one is
   switched and confirmed. Not acceptable.
Wanted: each tap gets an immediate cheap refresh (epd_fastest, black/white, ugly is fine) showing the
requested state + pending mark; input stays live, further taps on other widgets are accepted and sent
(user lane, in order) without waiting for earlier answers; the final clean redraw comes when HA confirms.
Step 1 is an investigation (what blocks the next tap, why the pending look never shows), see Notes.
Out of scope: the wake look (task 127).

## Files to touch
- decided after the investigation (likely lib/ui_state, lib/session, src/app_input.cpp, src/present)

## Acceptance criteria
- [ ] Native test: N taps on N different widgets within 1 s produce N pending widgets and N user-lane commands
      without any confirmation in between.
- [ ] Tap -> fastest partial of the tapped widget is started before any network work for that tap (log timing).
- [ ] Input is polled while a refresh or a command is in flight (no blocking EPD wait / HTTP in the input path).
- [ ] Pending look is drawn and stays until confirm/fail; confirm redraws the widget (final look).
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] Device: 5 lights tapped within ~2 s all show pending at once and all switch (log `tap_to_call_ms`).

## Notes
Investigation (tester, 2026-11-26; full evidence was in the session scratchpad, summary here):
- Pending look is never drawn: `AppState::widgetData` sets only `d.optimistic`; `d.pending` is set only by the alarm
  keypad (036 removed it). The call usually completes during the first (blocking) refresh, so the confirm frame is identical.
- Blocking: `present::waitAndCheck` blocks the loop 150-900 ms per rect (quality after a full refresh because the panel
  holds greys); a touch whose press+release falls inside the wait is lost (edge flags per `M5.update`); one input event
  per loop pass; dimmable lights defer to release (a lost release lets the next press overwrite the armed point);
  burst clean (GC16 1.5-2.5 s) is not gated on pending commands; session runs one Call at a time and FetchAll/FetchConfirm
  in flight delay later taps.

Decisions (architect, 2026-11-26):
- D1 Input is never lost: touches are sampled into a small ring buffer also inside BUSY waits (poll in the wait loop);
  the loop drains all buffered events per pass. Refreshes stay blocking otherwise (no async IT8951 use: too risky).
- D2 Tap feedback is always `epd_fastest` of the tapped widget, also over greys (user accepts ugliness); the tapped rect is
  refreshed first, before status rects. The pending mark (STYLE Pending: dashed 3 px ring/border, no hourglass needed)
  is drawn while the driver mark is Pending; confirm/fail always redraws the widget (final look, normal mode rules).
- D3 Burst clean and idle full wait until no command is pending and no tap for 2 s.
- D4 Deferred tap: a new press while a point is armed fires the armed point first (never overwrite).
- D5 Session: a user Call never waits behind FetchAll/FetchConfirm; queued taps go out as the next call right after the
  current response (one connection, one in flight is kept). Confirm fetches don't hold the user slot.
Split: 126a = D1-D4 (src/, lib/ui_state, render, present); 126b = D5 (lib/session only, worktree).
