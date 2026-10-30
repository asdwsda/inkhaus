# 058: Faster visual feedback on wake (investigation + fix)

Status: todo
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
