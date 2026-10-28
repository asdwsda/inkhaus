# 029: Wake log on the SD card (battery test build)

Status: in-progress
Depends on: 024
UI spec: none
Needs device: yes (battery test)

## Scope
docs/battery-test-plan.md Method A: with `-DWAKE_LOG_SD` (new env `m5paper-batterytest`, extends
m5paper), append one CSV line per wake to `/wakelog.csv` on the SD card at sleep entry: epoch,
wake reason, session kind, battery mV, awake ms total and per phase (display start, Wi-Fi connect,
fetch incl. TLS, refreshes), fetch result, refresh count. A header line when the file is created;
cap the file at 1 MB (stop appending, log once). Plus a serial command `wakelog` (debug/test builds
only) that streams the file framed like `shotz`, and `tools/wakelog.py` to save it as CSV.
Never log secrets. Off in the normal build (no SD access on normal wakes).

## Files to touch
- new src/wakelog.{h,cpp}; src/sleep.cpp (one call at sleep entry); src/main.cpp only for phase
  timestamps if not already available; platformio.ini (new env); tools/wakelog.py; pure CSV
  formatting in lib/app_logic with a test

## Acceptance criteria
- [ ] CSV formatting unit-tested; `pio test -e native` and all builds pass (incl. the new env).
- [ ] The normal m5paper build contains no SD access on the wake path (code review / grep).
- [ ] On device (later): a few wakes produce lines; `tools/wakelog.py` fetches them.

## Notes
