# 016: Software restart hangs in panelSignature()

Status: todo
Depends on: 014
UI spec: none
Needs device: yes (no user interaction; flashes and serial only)

## Problem
Found in task 014 round 3. After `ESP.restart()` (warm boot: the IT8951 kept power), the first
`readRectRGB` inside `panelSignature()` never returns; the log ends at `t_sig_sw_ms=99`.
Normal resets (EN/RTS, power-on) are cold and work. Any software restart, panic or watchdog
reset therefore leaves the device hung with the CPU awake. That's a battery drain and a dead
remote. Suspect: the controller is left in a burst-read state (`MEM_BST_RD_S` without
`MEM_BST_END`) or BUSY never releases, but this is unconfirmed. ESP32 deep-sleep wake (touch
mode) is also a warm boot for the controller, so the same path will be hit there.

## Scope
- Find the root cause by reading Panel_IT8951 (read path, BUSY handling) and with logs.
- Fix it so warm boots complete. Options in preference order:
  1. a correct protocol fix (end the burst read, wait for BUSY/ready properly);
  2. a bounded read with a timeout and a recovery path (controller reset, then treat as cold);
  3. as a last resort, skip the signature on warm boots and use the stored record alone.
     Document what is lost.
- Make every BUSY wait in our code bounded (timeout + log + recovery).
- Invalidate the screen record in NVS before a full draw starts, and write the new one after it
  finishes, so a power loss mid-draw leads to a full draw next time (tester finding, task 014).
- Enable the ESP32 task watchdog for the setup path, so any future hang ends in a reset
  instead of a powered, dead device. Make sure a watchdog reset doesn't loop forever: after 2
  consecutive watchdog resets, boot with the stock start-up and a full redraw.

## Files to touch
- src/boot.cpp, src/boot.h, src/main.cpp
- lib/screen_track/* and test/test_screen_track/* (only if the decision logic changes)

## Acceptance criteria
- [ ] With a `-DBOOT_TEST_RESTART` build, the warm boot completes, logs `controller=warm`, and
      the display start-up is under 200 ms (serial evidence).
- [ ] Forced hang test (a debug flag that blocks in setup): the watchdog resets the device, and
      after 2 such resets the stock fallback boots (serial evidence).
- [ ] Every BUSY wait in src/ has a timeout (code review).
- [ ] The record is invalidated before a full draw (test or code review).
- [ ] The cold-boot behaviour from task 014 is unchanged: `draw=partial`, box-only refreshes
      (serial evidence; visual check by the user only if timings change).
- [ ] `pio test -e native` and the m5paper/probe/epdtest builds pass.

## Notes
