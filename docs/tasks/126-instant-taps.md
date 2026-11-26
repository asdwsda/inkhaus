# 126: Instant tap feedback, rapid taps on several widgets

Status: todo
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
