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

### 2026-11-02 tester (code reading, no device)
- Modes: M5GFX maps epd_fastest=DU4, epd_fast=DU, epd_text=GL16, default/epd_quality=GC16 (Panel_IT8951.inl:487-492). Scheduled-wake and interactive changed rects both go through present.cpp refreshRect (:258-): epd_fastest (DU4, :268-270) if g_panel has only pure black/white in the rect (panelHasGrey :198), else one epd_quality (GC16, :264-266). Pushes are thresholded b/w (:265, kQualityPushesGrey=false :36), so after the first GC16 a rect stays b/w and later redraws are DU4. Graphs/status/sleep badge: no special case, same rule (main.cpp:926 Partial show). Full/ghost/periodic/modal use GC16 (:229-241, :544). No DU or A2 anywhere in src.
- Ghost debt (:562, :582) triggers a GC16 full only after N partials / area %, so a scheduled wake with a few DU4 rects never clears them (that is candidate c).
- Deep sleep, normal build: controllerStandby() -> STANDBY 0x0002 via setPowerSave(true) (sleep.cpp:279, boot.cpp:290-299); EPD_SLEEP_OFF build: SLEEP 0x0003 (sleep.cpp:275, boot.cpp:301-309). GPIO2 held high via pad hold + RTC hold (sleep.cpp:~285-296), so the board rail stays on. No src code touches VCOM or the TPS65185 (no setVCOM/getVCOM in src; M5GFX only calls setVCOM in init if _vcom>0, Panel_IT8951.inl:186). Nothing else drives the panel supply; any bias in sleep comes from the IT8951/TPS65185 own state (we only choose STANDBY or SLEEP), and whether the TPS goes to standby is up to the IT8951 (unknown, not controllable from our code).
- E1 (Power off, Updates Off), E2 (Deep sleep, Updates Off), E3 (Deep sleep, Updates 30 min, m5paper-nofull) all work with existing settings: Updates = refresh_interval_min 0/30 (settings.h:34,65), sleep mode in Settings. For E2 use plain m5paper (or nofull: with Updates Off needPeriodicFull needs scheduled=true, sleep_logic.cpp:125, so it never fires anyway). E3 needs env m5paper-nofull (-DNO_PERIODIC_FULL, platformio.ini:82-86). E1: plain m5paper's Power off sleep really drives GPIO2 low (sleep.cpp:216-221, SLEEP_NO_POWER_OFF is only for test builds; gpio2_low path :248-); on USB it falls back to deep sleep (:262-271), so run E1 on battery. GC16-partials variant of E3 needs a code change (refreshRect), no flag exists.
- 2026-11-02 (user): (a) ruled out (panel kept its image powered off). Suspect (c). Quick stress test first instead of E1-E3:
  standalone env `drifttest` (like `epdtest`): GC16 full of a test image (white bg, black text, a few grey/b-w blocks), then
  N partial refreshes of one small rect (toggle a box), awake, on USB. Phase A: N=100 with DU4 (= 100 hourly wakes).
  Pause for a photo (serial prompt or rocker). Phase B: GC16 full, N=100 with GC16 partials. Phase C (time control): GC16 full,
  same duration, no refreshes. If A greys and B/C don't -> (c) confirmed, fix = GC16 on scheduled-wake partials.
- 2026-11-02 18:10 stress run 1 (drifttest e662c34, USB, awake): A 100x DU4 avg 140 ms, B 100x GC16 avg 452 ms, C 200 s idle.
  Photos assets/092-stress-{A-du4,B-gc16,C-idle}.jpg. Inconclusive: user thought B maybe darker; lighting/exposure differs per
  photo, all three backgrounds look similar light grey. No clear drift from 100 refreshes of one small rect in 200 s.
- Architect: the 16:10 look-test run was in Power off mode (board unpowered between 30 min wakes), and (a) is ruled out
  (power-off without wakes keeps the image). So between wakes nothing drives the panel; the drift must come from what each
  wake does: (c) partial refreshes, or (d) the IT8951/TPS65185 power-up + init on every wake. 100 refreshes in one powered
  session do not exercise (d) or the time between refreshes.
- Run 2 plan (drifttest v2): every phase ends with a GC16 redraw of the right half only -> one photo compares drifted left
  vs fresh right under the same light. Phases: A many DU4 zones across the screen (user idea), B same with GC16,
  D N controller power cycles (IT8951 SLEEP + hw reset re-init, and TPS65185 rails up/down if reachable) with no pixel change.
