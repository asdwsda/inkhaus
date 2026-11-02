# 093: Task watchdog restart on a scheduled wake

Status: todo
Depends on: 083, 091
UI spec: none
Needs device: yes

## Scope
Wake log 2026-11-02 17:00:40 (m5paper-epdoff build, Sleep mode Power off, Updates 30 min): reset_reason=TASK_WDT inside
setup; the 16:30 rtc_alarm wake was fine. 083 Notes already saw a 15 s task_wdt hang when starting without the IT8951
hardware reset on a SLEEPing controller. Find where it hangs (likely BUSY wait on the IT8951 after an epdoff sleep, or Wi-Fi/
history on battery), whether the normal build can hit it, and fix (bounded waits, no 15 s blocking without feeding the WDT).

## Files to touch
- src/boot.cpp, src/main.cpp, src/sleep.cpp (as found), test/

## Acceptance criteria
- [ ] Notes: hang location (file:line) with evidence.
- [ ] No blocking wait in the wake path can exceed the task WDT; a stuck IT8951 ends in a logged error + recovery, not a WDT reset.
- [ ] Device: 10 scheduled wakes in a row (short interval) on the affected build without TASK_WDT in the wake log.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
