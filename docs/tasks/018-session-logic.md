# 018: Session logic: Wi-Fi, HA fetch, action queue (PC-only part)

Status: done
Depends on: 004, 005
UI spec: none
Needs device: no (wiring to Wi-Fi and HTTP on the device is a later task)

## Scope
A pure state machine in a new module `lib/session` that runs one wake session, driven by
events and a millisecond clock, emitting commands. The device code later only executes the
commands (Wi-Fi connect, HTTP via `ha_client`, render, sleep).
- **Inputs (events):** session start (wake reason from `lib/power`), Wi-Fi connected/failed,
  state fetch result (ok + changed ids / error kind from `ha_client`), service call result,
  user action (entity ids + `ha_client` action), user input activity, tick.
- **Outputs (commands):** connect Wi-Fi, fetch states, call service, show status (connecting /
  online / offline / auth error), mark widgets pending / confirmed / failed, request a render,
  ready to sleep.
- **Action queue:** taps before Wi-Fi is up are queued and sent once connected; per entity,
  the last action wins (two lamp toggles cancel out: send nothing); actions are sent in order
  of first tap.
- **Confirmation:** after a successful service call, fetch the affected entities again (with a
  short delay, e.g. 500 ms; HA updates states asynchronously) and confirm when the state
  matches the expectation; otherwise retry the fetch twice, then mark the widget "failed"
  (spec: failed badge) and show the real state.
- **Failures:** Wi-Fi failure gives offline (the UI keeps the last known states, marked stale);
  `HaError::Auth` gives an auth error (no retries); network errors retry once with the other
  base URL (already inside ha_client) and then offline.
- **Scheduled refresh session:** connect, fetch, request a render only if something changed,
  then ready to sleep; no input handling.
- **Sleep:** "ready to sleep" uses `lib/power`'s IdleTimer (busy while the queue or a request
  is in flight; hard cap) and returns the sleep plan.

## Files to touch
- lib/session/src/* (new)
- test/test_session/* (new)

## Acceptance criteria
- [ ] Interactive session happy path (start → connect → fetch → render → tap → call → confirm →
      idle → sleep) as a test with a fake clock.
- [ ] Taps before Wi-Fi are queued and sent after connect; toggling twice sends nothing (tests).
- [ ] Confirmation retry and the "failed" outcome (tests).
- [ ] Wi-Fi failure → offline, stale, still sleeps; auth error → no retry, error status (tests).
- [ ] Scheduled refresh session: no render if nothing changed; sleeps in both cases (tests).
- [ ] The hard cap ends a session even with a request hanging (test).
- [ ] No Arduino includes; `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Runs in a git worktree in parallel with task 017 (device). Don't touch src/ or other lib modules.
- Result: `lib/session` SessionMachine (events in, commands out, ms clock): queue with
  last-action-wins, confirmation with 2 retries, offline/auth handling, scheduled sessions,
  hard cap, low battery. 37 tests including a 500-run fuzz test (one request in flight, nothing
  after Sleep). Built in a worktree, merged as one commit.
- Follow-up in task 019: explicit on/off actions and sending queued actions before the first
  fetch (the current version fetches first).
- For the device wiring: the loop must keep sending `tick` events (sleep is only evaluated on
  events); the device supplies the epoch and the UTC offset in `start`.
