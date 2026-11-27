# 139: GT911 sleep in Deep sleep / Power off modes (experiment)

Status: done, NOT adopted (was: after 138; user accepted the risk of a dead touch panel, 2026-11-27)
Depends on: 138, 035 (research in its Notes)
UI spec: none
Needs device: yes

## Scope
In rocker-only sleep modes the GT911 stays powered (`SLEEP: ... touch=awake`) though it can't wake the
device. Datasheet: ~1-3 mA awake vs ~0.1 mA asleep (UNVERIFIED). Its wake needs an INT pulse, but INT is
GPIO36 (input-only) and RST is not wired, so a sleeping GT911 may never answer again (035 Notes).
1. Experiment build (flag `-DGT911_SLEEP_TEST`): before rocker-only deep sleep, write 0x8040 = 0x05 over I2C
   (no INT drive). On wake, log whether the GT911 answers on I2C (0x5D / 0x14) and whether touches arrive:
   `BOOT: gt911_after_sleep=ok|noack`.
2. Device run on USB: set Sleep mode = Deep sleep, sleep, wake with the rocker, check the log and a tap.
   Repeat 3 times. If `noack`: try recovery (unplug USB + reset button; record what works).
3. If it answers every time and touch works: make it the default for rocker-only modes (not Touch modes),
   with a fallback: if the GT911 does not answer after a wake, never send the sleep command again (NVS flag).
4. Record the result and decision in Notes and ARCHITECTURE.md.

## Files to touch
- src/sleep.cpp, src/boot.cpp, platformio.ini (flag), docs

## Acceptance criteria
- [ ] Device log of 3 sleep/wake cycles with the GT911 answer state.
- [ ] If adopted: tap works after each rocker wake; `SLEEP: ... touch=sleep` in rocker-only modes;
      Touch modes unchanged; NVS fallback flag tested natively where possible.
- [ ] `pio test -e native` and `pio run` pass.

## Notes
- Result (2026-11-27, build env m5paper-gt911sleep, flag GT911_SLEEP_TEST, serial `sleepnow`, 20 s timer wake): the GT911 acks the 0x8040=0x05 write
  (addr 0x14) but after the wake it never answers again: cycle 1 noack (`touch_init=fail`, `gt911_after_sleep=noack`), cycles 2 and 3 the
  sleep command itself got noack (still asleep). Evidence: scratchpad cycles.log. A serial DTR/RTS hardware reset (EN) brought it back
  (`gt911_after_sleep=ok addr=0x5d id=911`): so GT911 RST seems tied to EN, but a deep-sleep wake does not pulse it.
- Decision: do not adopt; touch stays awake in rocker-only sleep. No power-cycle recovery tried. Step 3 (NVS fallback) not built.
