# 058: Faster visual feedback on wake (investigation + fix)

Status: in-progress
Depends on: 024, 038
UI spec: none (may need a small ui-designer note if a new wake mark is drawn)
Needs device: yes

## Scope
After a touch/rocker wake from deep sleep the screen first changes ~3.2 s after the wake, so the user
doesn't know the wake worked. Evidence (2026-10-30 visual.log, t=91.1-94.5, times since wake):
`display_start_ms=1062`, `t_render_ms=1467`, then two `boot_diff` quality refreshes
(`wait_ms=748` + `439`), `t_first_draw_ms=3210`; Wi-Fi connected only ~0.2 s later.
Goal: a visible change within ~1 s of the wake (target to be measured), without extra ghosting.
Investigate and pick from (measure each):
- Why the display start takes ~1 s (M5.begin/EPD init, power-up waits) and what can be cut or overlapped.
- Clear the sleep indicator first with one `epd_fastest` refresh of its small rect (~130 ms), before
  rendering the rest; merge the two boot_diff rects into one refresh.
- Start Wi-Fi/net task before or in parallel with the first draw.
- Any boot work before the first draw that can move later (settings, config, render of the full frame).
Out of scope: changing the sleep indicator's look (task 059).

## Files to touch
- src/boot.cpp, src/main.cpp, src/present.cpp, src/sleep.cpp (as found)

## Acceptance criteria
- [ ] Notes: timeline of a wake with the cost of each step, and the chosen changes with a reason.
- [ ] Device: `t_first_draw_ms` (or a new `t_wake_feedback_ms`) clearly lower than 3210 on a touch and a
      rocker wake; the indicator disappears without ghosts; the final screen is unchanged.
- [ ] No regression in awake time per scheduled refresh (`awake_ms` in the wake log).

## Notes
Analysis (firmware-dev, read-only, 2026-10-30; full: session scratchpad 058-analysis.md). Times since t_pre_begin:
- 0-856 IT8951 ready wait (controller cold after deep sleep: RST GPIO23/CS GPIO15 not held, hypothesis);
  856-1062 SYS_RUN/dev info/panel+touch init; -1180 M5.begin, settings, SD wakelog, config;
  -1467 present::begin (sprites ~195) + compose (~90); -1850 full push to controller (296) + copy prev;
  -2600 boot_diff #1 status text (748, cold +300); -3050 boot_diff #2 = sleep indicator (439); -3210 close/log; Wi-Fi starts after.
- Proposals (gain/risk order): 1) merge/reorder boot_diff rects, indicator first (~440 ms, low risk);
  2) clear the 059 badge rect right after display init, non-scheduled wakes, overlap CPU work with the waveform (~1.5 s, medium);
  3) keep the controller warm in deep sleep by holding GPIO23/15 (up to ~1 s more; needs current measurement: architect);
  4) don't refresh the transient "connecting" status text in the first frame (UI decision);
  5) start Wi-Fi earlier (~1.9 s earlier connect); 6) small trims (100-200 ms).
- Instrumentation to add: t_boot_abs_ms, display_start/boot_present/t_render splits, t_wake_feedback_ms(+_done), t_wifi_start/up_ms.
- Order: 1 + instrumentation, 3 as experiment, 2 (after 059 fixes the badge rect), 5; 4 after a UI decision.
- User (2026-10-30): proposal 4 approved: on wake keep the last known status text instead of redrawing a transient
  "connecting" state in the first frame.
- Decision for part 2 (architect, 2026-10-30):
  - Proposal 2: go. On touch/rocker wakes (not RTC scheduled), right after display init + rotation, fill
    `sleepBadgeRect(o)` PAPER and refresh it with `epd_quality` (cold controller: unknown previous image, so no
    `epd_fastest`), no wait; do settings/config/compose CPU work during the waveform; waitDisplay before any
    controller push or SD write. Log `t_wake_feedback_ms` / `t_wake_feedback_done_ms`. bootFrame must not
    refresh the badge rect again unless the final content there is not plain paper (landscape tab slot 5): treat
    the rect as paper in the diff baseline.
  - Proposal 3 (hold GPIO23/15 in deep sleep): separate later task; needs a standby current measurement and a ghost
    check on the device first. Not in 058.
  - Proposal 4: go (first frame shows the last known status text, no transient "connecting" refresh).
  - Proposal 5: go: start the session/Wi-Fi right after config load; the net task must not touch the display or SD
    while a waveform runs (check the existing locks).
  - Proposal 6 trims: only the SD wakelog write after the first draw; skip the rest.
- Part 2 implemented (firmware-dev, PC only; device check pending): early badge-pill clear (`present::earlyClear`, quality, no wait)
  right after rotation on non-scheduled deep-sleep wakes; bootFrame waits first and adopts badge-only tile diffs
  (`screen_track::adoptClearedTiles`, native test); first frame keeps the cached status (`g_keepStatus`); session/Wi-Fi start moved
  before frame alloc; wakelog boot line written after the first draw (`begin(..., deferWrite)`, `writeDeferred`).
  New logs: `t_wake_feedback_ms`, `t_wake_feedback_done_ms`, `early_clear_paper`, `early_clear_adopted_tiles`.
