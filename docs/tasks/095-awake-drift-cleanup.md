# 095: Clear grey drift during long awake sessions

Status: PC done (tester review passed 2026-11-03; device check pending)
Depends on: 092
UI spec: none
Needs device: yes

## Scope
User 2026-11-02: plugged in with Keep awake On USB, the screen greyed within the session: HA updates and taps are partials,
and 092 only cleans up on scheduled wakes. While awake: if partial debt is set and there was no input for 5 min (constant),
do one GC16 full of the current frame, then clear debt. Not while a popup/Settings/OTA is shown or input is pending; restart
the 5 min on every input. Also applies to Keep awake Always. Out of scope: sleep entry (no flash, 092 decision).

## Files to touch
- lib/app_logic/ (decision, natively tested), src/main.cpp or src/present.cpp (hook)
- test/

## Acceptance criteria
- [ ] Native test: full due iff debt && idle >= 5 min && no modal; input resets the timer; no repeat without new debt.
- [ ] Device: on USB with Keep awake On USB, after HA updates and 5 min without taps, the log shows one `full_idle` GC16 and
      the screen is white; no further fulls while nothing changes.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
