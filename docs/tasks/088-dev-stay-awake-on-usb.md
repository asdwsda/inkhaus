# 088: Dev builds stay awake on USB

Status: todo
Depends on: 085, 081
UI spec: none
Needs device: yes

## Scope
User 2026-11-02: on dev/debug firmware the device should not need a wake-up while it is plugged in.
- New build flag `-DDEV_STAY_AWAKE_USB`, set in `env:m5paper-debug` only. With it, the effective keep-awake is at
  least On USB, whatever the stored `keep_awake` setting (Always stays Always). The stored setting is not changed.
- The release env `m5paper` and the measurement envs (batterytest, epdoff, nofull, poweroff) are unchanged: they must
  still sleep on USB so sleep paths stay testable.
- "Sleep now" (2.5 s rocker hold and the Settings row) still sleeps in the debug build; unplugging lets the normal idle timeout apply.
- Boot log line `BOOT: dev_stay_awake_usb=1` in the debug build.
- Agent docs: `.claude/skills/m5paper-device/SKILL.md` says to flash `m5paper-debug` for normal dev/device work (and to
  use the release or a measurement env when a test needs sleep on USB); also update the env comment in platformio.ini.
- Out of scope: any change to the release defaults or the settings UI.

## Files to touch
- platformio.ini, src/main.cpp (where keep-awake is evaluated), .claude/skills/m5paper-device/SKILL.md

## Acceptance criteria
- [ ] `pio run` (all envs) builds; `pio test -e native` passes.
- [ ] Debug build on USB with stored keep_awake=Off: no idle sleep after 2x the idle timeout; serial commands answer without a reset.
- [ ] Debug build: rocker hold 2.5 s still sleeps (and wakes normally).
- [ ] Release build `m5paper` on USB with keep_awake=Off still sleeps after the idle timeout (behaviour unchanged).
- [ ] SKILL.md and the platformio.ini comment updated.
