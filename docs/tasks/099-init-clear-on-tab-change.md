# 099: INIT clear on tab change (experiment, then default)

Status: in-progress
Depends on: 092, 095
UI spec: none
Needs device: yes (photos + timing; PC part first)

## Scope
The factory app (research: docs/tasks/assets/099-epd-research.md) wipes the panel with
the INIT waveform (mode 0) on every screen change, then shows the composed screen with one full GL16.
That is likely why it shows no grey drift. User decision (2026-11-03): an INIT wipe is allowed
on tab change only, never on taps, popups or scheduled wakes.

Experiment first, then make the winner the default:
- Tab-change cleanup variants, selectable at runtime on m5paper-debug by a serial command
  (e.g. `tabclean 0|1|2|3`, default stays the current behavior until the device check):
  0 = current (GC16 full), 1 = INIT clear + GC16 full, 2 = INIT clear + GL16 full, 3 = GL16 full only.
- INIT is not exposed by M5GFX: send the IT8951 display-area command with mode 0 directly, reusing the raw
  command access in src/boot.cpp (Panel_IT8951 internals), wait on BUSY via present::guardController /
  boot::waitBusyHigh. Whole screen only, x/w aligned to 4 px.
- Log per tab change: `refresh=tab_clean v=N init_ms=.. full_ms=..`.
- Partial debt (task 092) is cleared by variants 1-3 the same way as by today's GC16 full.
- Out of scope: INIT anywhere else; idle (095) and scheduled (092) fulls stay GC16; direct 4bpp pushes.

## Files to touch
- src/present.{h,cpp} (tab-change path), src/boot.{h,cpp} (expose a raw INIT display helper)
- src/debug_cmds (or wherever the m5paper-debug serial commands live)
- lib/ for any pure selection logic, test/ for it

## Acceptance criteria
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Release build behavior unchanged (variant 0) until the device result is in.
- [ ] Debug build: `tabclean N` switches the variant; each tab change logs the line above.
- [ ] docs/device-checks.md line: for each variant, 20 tab changes on a drift-prone tab (greys + text), user
      photo after; note init_ms/full_ms. Winner becomes the release default in a follow-up commit.
- [ ] UNVERIFIED tags on the raw INIT command parameters.

## Notes
- Expected cost: INIT ~1-2 s with a black/white flash; GL16 full is faster than GC16 but may ghost on
  small boxes (only used full-screen here).
- Decision owner after the device check: architect; record it in ARCHITECTURE.md.

- 2026-11-03 PC part done: lib/screen_track/src/tab_clean.h + test/test_tab_clean, boot::initClearWhole (raw 0x0034, UNVERIFIED), present tab-change detection via setLogTab (tab differs, Full, not popup close), debug `tabclean N`. Next: device check (docs/device-checks.md).

- 2026-11-03 device (camera + log): defaults stay burst = gc16, n=4, 2 s; tab change = variant 0 (GC16). INIT works (0x34 and 0x37, 0x37 ~35 ms faster) but costs 1.1 s with no visible gain; kept debug-only.
