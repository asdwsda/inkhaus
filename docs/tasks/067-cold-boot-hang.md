# 067: Intermittent cold-boot hang after BOOT: controller_id=

Status: in-progress
Depends on: none
UI spec: none
Needs device: yes

## Scope
Found in the 2026-10-30 device session: about 2 of 14 serial hardware resets hang right after `BOOT: controller_id=`;
the task watchdog fires and the stock-fallback boot recovers. debug.log has 3 earlier `reset=crash` wakes.
Find the cause (EPD controller init / BUSY wait / SPI bus with SD) and fix it, or bound it so the first boot never hangs.

## Files to touch
- src/boot.cpp (as found)

## Acceptance criteria
- [ ] Root cause in Notes, with log evidence.
- [ ] Device: 30 consecutive serial resets and 10 deep-sleep wakes without a watchdog reset.

## Notes
- Timebox (user, 2026-10-30): if the hang cannot be reproduced, or only very rarely, stop the investigation: keep the markers/bounds that help catch it later, record the reproduction rate, and close the task without the 30-reset criterion.
