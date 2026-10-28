# UI flow storyboard (animated GIF)

Portrait, 540 x 1040 per frame: device image (0,0,540,960) plus caption bar
(0,960,540,80), fill #CCCCCC, F24 INK text centred at (270,1000). Geometry, ids and
values come from STYLE.md, layout.md, widgets.md, settings.md (revision 4) and the
reference mockups `tab-portrait.svg`, `tab-portrait-asleep.svg`, `settings-portrait.svg`.

Frame types:
- **state**: a visible UI state.
- **tap**: a state plus the fingertip marker (`id="tap-marker"`: grey #888, 40 px outer
  diameter, outline only).
- **partial**: the NEXT state, with the changed region(s) inverted (black/white swapped,
  done with an SVG colour-invert filter clipped to the dirty rects) and a thin dashed
  #888 outline around each region. 120 ms.
- **full**: all black (150 ms), then all white (150 ms), then the new state.

Widget ids (`w-N`, `w-N-frame`, `w-N-*`) are those of the tab mockups (N = config order
index of the worked example). Refresh-flash frames (black, white) have no widgets.
`pager-*` and `tap-marker` ids are outside the `w-N-` convention on purpose.

## Frame table

| # | File | Caption | ms | Type | Refresh illustrated |
|---|------|---------|----|------|---------------------|
| 01 | 01-asleep.svg | Asleep – press the side button | 1500 | state | none (frozen, sleep indicator shown) |
| 02 | 02-wake-partial.svg | Partial refresh | 120 | partial | sleep indicator region (292,3,112,48) cleared |
| 03 | 03-woken.svg | Awake – sleep chip cleared | 1500 | state | none |
| 04 | 04-label-partial.svg | Partial refresh | 120 | partial | refresh label (44,11,240,32) |
| 05 | 05-updated.svg | Fetch done – updated 14:32 | 1500 | state | none |
| 06 | 06-tap-lamp.svg | Tap: Floor lamp | 600 | tap | none |
| 07 | 07-lamp-pending-partial.svg | Partial refresh | 120 | partial | toggle widget rect (276,300,248,104) |
| 08 | 08-lamp-pending.svg | Floor lamp pending | 1500 | state | none |
| 09 | 09-lamp-on-partial.svg | Partial refresh | 120 | partial | toggle widget rect (276,300,248,104) |
| 10 | 10-lamp-on.svg | Floor lamp on | 1500 | state | none |
| 11 | 11-tap-shutter.svg | Tap: Shutter up | 600 | tap | none |
| 12 | 12-shutter-pending-partial.svg | Partial refresh | 120 | partial | tapped button (296,88,64,64) |
| 13 | 13-shutter-pending.svg | Shutter pending | 1500 | state | none |
| 14 | 14-shutter-opening-partial.svg | Partial refresh | 120 | partial | Shutter state box (28,122,224,30) + up button (296,88,64,64); group state box (28,238,224,30) + up button (296,204,64,64) |
| 15 | 15-shutter-opening.svg | Shutter opening 40% | 1500 | state | none |
| 16 | 16-tap-plus.svg | Tap: Thermostat + | 600 | tap | none |
| 17 | 17-target-pending-partial.svg | Partial refresh | 120 | partial | "+" button (448,552,64,64), target box (312,560,136,48), badge (212,556,24,24) |
| 18 | 18-target-pending.svg | Target 22.5° pending | 1500 | state | none |
| 19 | 19-target-set-partial.svg | Partial refresh | 120 | partial | same three rects as 17 |
| 20 | 20-target-set.svg | Target 22.5° confirmed | 1500 | state | none |
| 21 | 21-tab-partial.svg | Partial refresh | 120 | partial | tab bar (0,880,540,80) |
| 22 | 22-tab-highlight.svg | Rocker down – tab Whole house | 1500 | state | none (content still Living room) |
| 23 | 23-full-black-tab.svg | Full refresh | 150 | full (black) | whole screen, tab change |
| 24 | 24-full-white-tab.svg | Full refresh | 150 | full (white) | whole screen |
| 25 | 25-whole-house.svg | Tab: Whole house | 1500 | state | none |
| 26 | 26-full-black-settings.svg | Full refresh | 150 | full (black) | whole screen, Settings enter |
| 27 | 27-full-white-settings.svg | Full refresh | 150 | full (white) | whole screen |
| 28 | 28-settings.svg | Settings (rocker long push) | 1500 | state | none |
| 29 | 29-tap-interval.svg | Tap: Refresh every | 600 | tap | none |
| 30 | 30-interval-partial.svg | Partial refresh | 120 | partial | value rect (256,268,252,88), hint rect (32,318,220,24), unsaved chip (404,11,120,32) |
| 31 | 31-interval-changed.svg | Refresh every 120 min – unsaved | 1500 | state | none |
| 32 | 32-tap-back.svg | Tap: Back | 600 | tap | none |
| 33 | 33-full-black-back.svg | Full refresh | 150 | full (black) | whole screen, Settings exit |
| 34 | 34-full-white-back.svg | Full refresh | 150 | full (white) | whole screen |
| 35 | 35-whole-house-back.svg | Back – Whole house | 1500 | state | none |
| 36 | 36-sleep-partial.svg | Partial refresh | 120 | partial | sleep indicator region (292,3,112,48) drawn |
| 37 | 37-asleep-end.svg | Idle timeout – asleep | 2500 | state (final) | none |

Total 28720 ms (14 states x 1500, 5 taps x 600, 11 partial x 120, 6 full x 150,
final 2500).

Ghost debt (STYLE.md section 10): 9 partial updates (frames 02..21) before the first full
refresh, below the limit of 12; the summed partial area is far below 150 % of the screen.
Tab change, Settings enter and Settings exit are the three full refreshes.

## Decisions the specs did not cover (or that differ from the brief)

1. **Refresh every: 60 min becomes 120 min, not 30 min.** settings.md rule 7 cycles
   forward only (0, 15, 30, 60, 120, 240); one tap on 60 gives 120. Reaching 30 takes five
   taps. Frames 30..32 show 120 min (pip 4 filled).
2. **No stale chip on wake.** Stale = age > 2 x refresh interval (120 min at 60 min);
   14:05 to 14:32 is 27 min, so the label stays plain until the fetch ends.
3. **Wake sequence**: the connection icon "connecting" single dot (STYLE.md 5.3) is a
   transient of a second or so and is not drawn; frame 04 inverts only the label.
4. **Story state before the shutter tap**: Shutter "Closed" (no button filled). The
   reference mockup shows "Opening 40%"; that state is the result of the story.
5. **Group "Downstairs shutters" follows the Shutter.** `cover.living_room_shutter` is a
   member of `downstairs_shutters` (config.example.yaml), so the same fetch turns the
   group into "Opening" with its up button filled (widgets.md section 2). Hence the two
   extra dirty rects in frame 14. It is NOT pending (only the Shutter was tapped).
   The same member makes "All shutters" on the Whole house tab show "Opening".
6. **Pending on an INK-filled toggle (frame 08)**: a dashed INK border would be invisible
   on the INK fill. The 3 px dashes (8 on / 6 off) are drawn in PAPER, inset 4 px, as
   `w-4-pend-btn` (ends in `-btn`, so the checker skips it). The hourglass badge is PAPER
   at (212,57). Spec gap; needs a rule in STYLE.md section 9.
7. **Floor lamp state text** is plain `On` (brightness unknown), both pending and confirmed.
8. **Thermostat step 0.5**: 22.0 to 22.5 (target_temp_step 0.5 assumed, one decimal).
   Pending badge at (W-312,24) = (196,24) widget-relative; hourglass INK.
9. **Tab bar** keeps the mockup's 5-tab configuration (Living room, Whole house, Kitchen,
   overflow `1/2`) although config.example.yaml has only two tabs. Tab focus frame (2 px
   PAPER, inset 4, radius 4) appears from the first rocker event (frame 21) and is gone
   after leaving Settings (TAB mode without ring, layout.md 7.2).
10. **Whole house layout (frames 25, 35..37)**: 9 widgets; the 9th (wide cover) does not
    fit after the 8th in 7 rows, so the second pass with 6 rows applies and the pager is
    shown ("1 / 2", previous dotted OFF, next active). Page 0 = w-1 group (All shutters),
    w-2 Night mode, w-5 Outside (fills the gap beside w-2), w-3 Bedroom climate, w-4
    Kitchen climate, w-6 weather (config `size: 2x2` clamped to 2x1), w-7 Vacuum. Page 1
    (w-8 person.alice sensor, w-9 cover) is not shown. Invented values: Bedroom
    19.5 / 52 % / idle (pause icon) / target 19.0; Kitchen 20.5 / 48 % / heating (flame) /
    target 21.5; Vacuum "Docked", 85 %, dock button filled. Vacuum battery group:
    S battery icon at (114,57), `85%` F18 at x = 142 (new `batt` S glyph, not in the
    icon list yet).
11. **Settings page values**: saved values Portrait / Button / 60 min / 20 s, so no
    touch-wake warning or note; HA row `local, ok 14:32`; Config `OK`, errors `0`
    (the reference mockup shows an offline HA and 3 errors). Focus ring on row 1 (entry
    focus = stop 1) because Settings was opened with the rocker; the later taps do not
    move it.
12. **Tap marker positions**: Floor lamp (400,352); Shutter up button (328,120);
    thermostat "+" (480,584); Refresh every row (300,312); Back (440,912).
13. **Dirty rects rounded out** to whole pixels (frame 14: state boxes y 122.6..151.4
    becomes 122..152). The label region (44,11,240,32) is the widest stale-chip rect
    (STYLE.md 5.3).
14. **Rocker and side button** are not drawn (outside the 540x960 image); captions carry
    the action.
15. **Widget checker**: frames 28..32 have no `w-N` widgets, so `check_mockups.py` reports
    "no widget frames" for them (same as settings-portrait.svg). Frames 23, 24, 26, 27, 33,
    34 are exempt.

Revision 5 (2026-10-28): all dynamic text in the frames (state, condition) is pure
black and the window sensor reads `Open`. Partial frames are feedback refreshes in
fastest mode over black/white content, with no clean-up pass; the full refreshes
are GC16 (STYLE.md section 10).
