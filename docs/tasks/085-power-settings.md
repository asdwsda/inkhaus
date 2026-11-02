# 085: Power settings: sleep mode, updates, keep awake, battery estimate, state indicators

Status: PC part done (device check pending)
Depends on: 084 (power-off), 081 (stay awake), 079
UI spec: docs/ui/settings.md (Sleep & wake page, new revision) + docs/ui/STYLE.md (status bar indicators) — ui-designer
Needs device: yes

## Scope
User 2026-11-02: full control over how much battery to trade for convenience; battery life first.

Sleep & wake page (replaces Wake mode, Refresh every and the Stay awake toggle):
1. **Sleep mode**, ordered by cost: `Power off` (rocker wakes; lowest) / `Deep sleep`
   (rocker wakes, faster) / `Touch` (tap wakes) / `Touch & rocker`. Migrates the existing wake_mode setting.
2. **Updates while asleep**: `Off` / 30 min / 1 h / 3 h / 6 h / 12 h (existing refresh_interval; 0 = Off).
   In Power off it uses the RTC power-on from 084; if 084 can't make that work, Power off forces Off (shown).
3. **Keep awake**: `Off` / `On USB` / `Always`. `Always` = never sleep (manual, for when it's docked).
   `On USB` only where USB is detected without extra awake time: in Power off mode via the power-off fallback
   (084 `usbAtSleep()`); in the deep-sleep modes only if the CH9102 RX probe (081 Notes) proves reliable,
   otherwise `On USB` is not offered there. The voltage heuristic from 081 is removed.
4. **Battery estimate** line at the bottom of the page, updated live as values change: "About N days per charge"
   (or hours/weeks), plus a small relative gauge. Computed in lib/ from a model: rest current per sleep mode,
   cost per scheduled update, assumed daily use (e.g. 10 wakes x 40 s), Keep awake Always = hours.
   Constants come from the wake-log data (scheduled wake ~6.7 s awake; deep sleep ~67 h/charge measured)
   and are marked as estimates until measured.

Status bar indicators (drawn before sleep, and while awake where relevant), one per state, designed by ui-designer:
- Power off (replaces Zz: the Zz badge doesn't fit a powered-off device), Deep sleep, Touch wake,
  Updates off / next update time, Keep awake active (awake on USB / always awake).
- Out of scope: measuring currents (no USB meter).

Pure logic (estimate model, setting migration, mode -> sleep plan) in lib/ with native tests.

## Files to touch
- lib/settings/src/settings.{h,cpp} (new fields, schema version + migration), lib/settings_ui/src/settings_ui.{h,cpp}
- lib/power/src/ (estimate model; remove UsbDetector heuristic), lib/app_logic/src/sleep_logic.* (plan per mode)
- src/sleep.cpp, src/main.cpp, src/render/ (indicators), src/settings_flow.cpp
- test/

## Acceptance criteria
- [ ] Native tests: migration from schema 2 keeps the user's wake mode and interval; each Sleep mode gives the
      right sleep plan; Keep awake On USB only offered where supported; estimate model monotonic (more costly
      settings never give more days) and matches hand-computed values for 3 sample configs.
- [ ] Settings page matches the spec and mockups; estimate updates while cycling values.
- [ ] Each state shows its indicator (device photos, user check).
- [ ] Power off + Updates Off: no wake except the rocker (wake log over a night).
- [ ] Keep awake Always: never sleeps; Off: behaviour as configured.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
- 2026-11-02 PC part done (agent): settings schema 3 (sleep_mode, keepawake; intervals 0/30/60/180/360/720; migration of
  wake_mode/stayusb/old intervals), lib/power/estimate.{h,cpp} (model + formatting), plan_sleep per mode, pill indicators
  (STYLE 5.6, region now 108 wide), Sleep & wake page with notes and estimate block, 081 UsbDetector removed (On USB =
  power::usbAtSleep()). Capabilities in settings.h (power_off, rtc_power_on, usb_probe_reliable) are compile-time and
  UNVERIFIED pending 084's battery test. src/sleep.cpp: Power off now powers off (no build flag), stores the next update
  epoch for the pill. Device checks in docs/device-checks.md.
- User 2026-11-02 (replaces Scope item 4): no battery-life estimate (no days/weeks; we don't know it precisely).
  Instead a simple "Battery use" bar on a 1..10 scale for the current set of settings (1 = Power off, Updates
  Off, Keep awake Off; 10 = Keep awake Always). The lib model still ranks configurations (monotonic), but only
  the 1..10 level is shown. Acceptance: "estimate model monotonic" -> "level monotonic, 1 and 10 at the ends".
- User 2026-11-02: the level must reflect all four inputs: Sleep mode, Updates while asleep (interval), Keep awake, Idle timeout. Native test: shorter update interval never gives a lower level; Updates Off vs 30 min differ by at least 1 level in Power off.
- User 2026-11-02, defaults: Sleep mode Deep sleep (rocker wake, no power off), Updates every 1 h, Idle timeout 20 s, Keep awake Off. (Overrides 'Power off default' in Scope.)
