# 090: Total time cap for an OTA job

Status: shipped v1.4.0, device check pending
Depends on: 089
UI spec: docs/ui/settings.md §8.6 (existing Failed look; no new UI)
Needs device: no (PC; device check optional, UNVERIFIED allowed)

## Scope
From the 089 review: the loop waits for the otajob task without a total deadline. HTTP connect/read timeouts
(8/10 s) and the 15 s stall limit bound each step, but a download trickling one byte every 14 s could keep the
device awake for a very long time. Add a total deadline per job:
- check (manifest): 60 s; install (download + verify): 5 min. Constants in lib/ota_logic (pure, unit-tested).
- When the deadline passes, the job aborts at the next read/step (cooperative cancel flag checked by the job,
  no task kill): `esp_ota_abort`, pending flag stays false, the row shows the existing Failed look with a
  short reason (`Took too long, not installed` / check: the existing check-failed text), input and sleep
  rules return to normal. The loop still waits for `done` (bounded by the step timeouts) before freeing anything.
- Log `OTA: job timeout kind=check|install after_ms=N`.
Out of scope: changing the step timeouts.

## Files to touch
- lib/ota_logic/src/ota_logic.{h,cpp}, src/ota.cpp, test/test_ota_logic/

## Acceptance criteria
- [ ] Native tests for the deadline decision (before / at / after, check vs install) pass; `pio test -e native` passes.
- [ ] `pio run -e m5paper` and `-e m5paper-debug` build.
- [ ] Code: cancel is cooperative; nothing frees the Job or its stack before `done`; on timeout the OTA partition is aborted and the running firmware is untouched.
- [ ] docs/device-checks.md line (UNVERIFIED): with tools/ota_test_server.py throttled (add a `--slow BYTES_PER_S` option), the install fails with the timeout text after 5 min.
