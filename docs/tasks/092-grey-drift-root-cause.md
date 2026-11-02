# 092: Grey drift: find the cause, replace the 3 h full refresh if possible

Status: todo
Depends on: 083, 091
UI spec: none
Needs device: yes (unattended runs + user photos)

## Scope
083 added a GC16 full refresh every >= 3 h as a mitigation. The drift was still visible on the epdoff build
(IT8951 SLEEP in deep sleep, 2026-11-02 16:10 run), so "IT8951 STANDBY bias" alone doesn't explain it. Candidates:
- (a) natural relaxation of the panel, worse for pixels last driven by a fast/partial waveform;
- (b) bias from the powered board in deep sleep (GPIO2 hold keeps TPS65185/panel supply up);
- (c) DC imbalance: every partial refresh drives VCOM over the whole panel, so non-updated pixels creep a little
      per scheduled wake.
Experiments (one device, sequential; each starts with a GC16 full of the same tab, then no touching; photo at 3 h):
- E1 Power off sleep, Updates Off: pure relaxation (no supply, no refreshes).
- E2 Deep sleep, Updates Off, normal build: (b) without (c).
- E3 Deep sleep, Updates 30 min, periodic full disabled (m5paper-nofull): (c).
- Also: do the redrawn graph rects use GC16 or a fast mode? If fast, test E3 with GC16 partials.
Fix by evidence, cheapest first:
- (c) only -> partials on scheduled wakes use GC16 (or a full GC16 on each scheduled wake that redraws anything;
  the panel is unseen while asleep, ~0.4 s extra per wake).
- (b) -> power the panel supply down in sleep (if a switch exists) or prefer Power off sleep.
- (a) only -> keep the periodic full; tune the period from E1 (time until drift is visible).
- Out of scope: waveform tuning.

## Files to touch
- src/present.cpp, src/sleep.cpp, lib/app_logic/ (refresh-mode rule), test/, platformio.ini (test envs)

## Acceptance criteria
- [ ] Notes: E1-E3 results (photos by the user, time until drift visible, wake-log mV/h).
- [ ] Chosen fix recorded in ARCHITECTURE.md with a one-line reason.
- [ ] After 8 h unattended with the user's settings: no visible drift (user photo).
- [ ] Native test for any changed refresh-mode rule; `pio run` and `pio test -e native` pass.

## Notes
