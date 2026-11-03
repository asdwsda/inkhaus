# 098: Native tests for UNVERIFIED device-check logic

Status: done
Depends on: 087, 090, 092
UI spec: none
Needs device: no

## Scope
Shrink the next device session by covering with native tests the logic behind these open items in
docs/device-checks.md:
- 092: scheduled wake with no changes -> no refresh; refresh debt cleared by a no-change scheduled wake;
  Updates Off + debt -> first frame `debt_full`; debt kept across power-off (NVS fake).
- 090: OTA job deadline (install > 5 min -> `Took too long`; manifest stall -> `Check failed: timeout` at 60 s).
- 087: battery gate (< 50 % off USB blocks install), low-memory skip, Wi-Fi-down error text.
Tests only. If something needs a small seam (pure function extracted, fake clock injected), keep it
minimal and behavior-preserving. Report real bugs instead of fixing them silently.
Out of scope: behavior changes; src/ beyond tiny seams.

## Files to touch
- test/
- lib/ only for minimal seams

## Acceptance criteria
- [ ] Each listed item has at least one named test, or a one-line reason it can't be simulated.
- [ ] `pio test -e native` and `pio run` pass.
- [ ] docs/device-checks.md: covered items note "logic unit-tested (098)"; device-only parts stay open.
