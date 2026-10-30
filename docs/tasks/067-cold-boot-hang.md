# 067: Intermittent cold-boot hang after BOOT: controller_id=

Status: done
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
- 2026-10-30 (firmware-dev): reproduced with temporary markers, 30 serial resets (poweron): 2/30 hung (runs 22, 28; logs /tmp/claude-1000/-home-petersz-priv-m5paper-ha-remote/d4c1342b-4ac2-458d-a42c-39bbce59b4de/scratchpad/runs_before.log). Both stop right after `controller_id=62915100` and `dbg_before_panel_init reset=0` (BUSY was high, no reset pulse), i.e. inside `Panel_IT8951::init(false)` (second GET_DEV_INFO + SYS_RUN + target addr); never reached `after_panel_init`. The 12-14 s capture ended before the 15 s watchdog, so the reset itself was not seen. Library waits there are bounded (4 s each) but sum past 15 s.
- Tried: closing the probe's read burst with MEM_BST_END before panel init (hypothesis from the read-state pitfall): 5/40 still hung in the same place (runs 9, 24, 35, 38, 39; /tmp/claude-1000/-home-petersz-priv-m5paper-ha-remote/d4c1342b-4ac2-458d-a42c-39bbce59b4de/scratchpad/runs.log). Reverted, not the cause. Root cause not found; rate ~5-12%, only on hardware reset (power-cycled controller), 0 hangs on any other path observed.
- Left in: 4 `BOOT: dbg_*` markers in src/boot.cpp (after_cfgbus, before_panel_init, after_panel_init, before_touch, before_m5begin) to localise future hangs. The existing watchdog + stock-fallback recovers. Closed per timebox; 30-reset/10-wake criterion not met. Next idea if wanted: run the probe's GET_DEV_INFO only (skip Panel init's repeat), or wait for BUSY after the 1.06 s start-up before the first command.
- 2026-10-30 round 2 (firmware-dev): one fix tried in `attachNoResetDisplay`: after the probe, if the controller was cold (BUSY wait > 100 ms), wait until 400 ms after BUSY went high (covers the ~1.06 s start-up), then the existing bounded BUSY wait, then `Panel_IT8951::init(false)`. No library fallback added (init's internal waits cannot be bounded from outside). Result: 30 hardware resets (all reset_reason poweron, all reached `touch_init`): 0 hangs (before: 2/30, 5/40). 0/30 is weak evidence (about 10 % chance at a 7 % rate) but nothing contradicts it; fix kept. Log: scratchpad 067-r2-resets.log. Deep-sleep wakes: not measured; the only wake sources are touch or the RTC alarm (next 15:00), and a temporary 5 s timer wake in src/sleep.cpp (not on the file list) was blocked. Needs the user's touch (10 wakes) or permission for the temporary edit.
- Closed under the timebox (2026-10-30):
  - Round 2 fix (cold-controller settle wait): 0 hangs in 30 resets, against 7 of 70 before.
  - Deep-sleep wakes were not measured separately. They are watched during the 071 device check: look for `BOOT: dbg_*` stopping before `dbg_after_panel_init`.
  - If a hang shows up again, reopen this task.
