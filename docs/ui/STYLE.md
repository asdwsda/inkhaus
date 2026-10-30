# UI style (shared values)

Owner: ui-designer. All screens and `docs/ui/layout.md` / `docs/ui/widgets.md` /
`docs/ui/settings.md` reuse the values below. Coordinates are px, origin top-left
of the screen as the user sees it (after rotation). Rect = (x, y, w, h).

Revision 5 (2026-10-28, approved on the real device; ARCHITECTURE.md 2026-10-28):
text that can change at runtime is pure INK (section 1), the refresh rules are
rewritten (section 10: fastest over black/white, one quality refresh over greys,
no per-tap clean-up, GC16 full refresh on tab/page change and ghost debt),
the rocker yes/no rule is in section 10b, binary sensors with device_class
window/door say Open/Closed (widgets.md section 8).

Revision 4 (type budgets): user feedback said a sensor value left its container and
the thermostat looked odd. Cause: vertical budgets were computed from centre lines
without real text box heights. Changes: text box = 1.2 x font size (section 2), 4 px
minimum between stacked boxes, at most 2 text lines per 1-row widget, unit runs
share a baseline (`baseline_left`), only the sizes F18/F24/F40/F56, SVG font family
`DejaVu Sans` only, new stacks in section 4b, per-widget budget tables in
widgets.md, toggle names always F24, climate and weather rebuilt, flame/window/pause
icons.

Revision 3 (compact, centre-line aligned): user feedback said widgets were too big,
content too small for its container and not vertically centred. Changes: touch
minimum 64 (was 80), gap 12 (was 16), cell height 104 (was 144), one internal
widget grid for every widget (section 4b), every text and icon placed by its
vertical centre line (section 2), new sleep indicator (section 5.5), battery
cluster opens Settings.

## 1. Grey levels

Level n is hex `n*0x11` (0 = black, 15 = white). Only these five are used.

| Name | Level | Hex | Use |
|------|-------|-----|-----|
| INK | 0 | #000000 | all dynamic text (see rule below), borders, "on" fills, icons |
| DIM | 4 | #444444 | static decoration only: never-changing captions and names (sensor name, Settings info labels) |
| OFF | 8 | #888888 | unavailable titles and icons, dotted borders, hairlines, dividers |
| TRACK | 12 | #CCCCCC | empty part of progress bars only (small areas) |
| PAPER | 15 | #FFFFFF | background, "off" fills, text on INK |

**Dynamic text is pure black.** Any text that can change at runtime is INK (0),
never DIM or OFF: state texts (`Closed`, `Opening 40%`, `On`, `Open`), values and
temperatures, the weather condition, vacuum status, humidity, Settings values,
the refresh label. On INK fills (toggle "on") it is PAPER. Stale, loading and
unavailable values are therefore also INK/PAPER (`-`, `n/a`); the stale state is
carried by the status-bar clock icon, the unavailable state by the dotted frame and
warn badge. Greys stay only for static decoration: dividers and hairlines,
never-changing captions (sensor/weather names, info labels), unavailable widget
titles and icons, badges. Reason: black text keeps nearly every update on the
fast path (black/white only); grey text needs the slower quality mode.

Rules: never use a grey as the only carrier of state (always shape or fill as
well); no large areas of grey 1..14 (uneven refresh, ghosting). Text on INK
fills is PAPER.

## 2. Fonts and vertical alignment

M5GFX built-in DejaVu vector sizes (px = nominal height).

| Name | Size | Use |
|------|------|-----|
| F18 | 18 | captions, status bar, tab labels, secondary lines, hints |
| F24 | 24 | widget names (never stepped down), state lines, climate current temperature line, unit next to an F40 value, button labels, Settings labels |
| F40 | 40 | climate target, weather temperature, sensor 1x1 value, pager "2 / 3", Settings values |
| F56 | 56 | sensor 2x1 value only (does not fit any 2-line stack) |

Only these four sizes exist (72 is reserved, unused). SVG mockups use
`font-family="DejaVu Sans"` and nothing else (no fallback list).

### Text box rule (applies to every widget)

A text line's **box is 1.2 x font size high, centred on its centre line**: F18 21.6,
F24 28.8, F40 48, F56 67.2. Rules: every box lies inside the widget content box
(frame minus 12 px = y 12..92 for H = 104); boxes of neighbouring lines in a stack
are >= 4 px apart; at most 2 text lines per 1-row widget (never a 3-line stack).
Each widget has a vertical budget table in widgets.md (font, cy, box top, box bottom,
gaps, total <= 80).

### Centre-line rule (applies to every screen)

**Every text and every icon is placed by its vertical centre line**, never by a
top or baseline.
- Text: M5GFX datum `middle_left`, `middle_center` or `middle_right` at the
  centre-line y. Specs give `cy` (the centre line), not the top of a box.
- Icon: drawn in a square whose centre is the centre line (`top = cy - size/2`).
- An icon and the text it labels share one centre line.
- **Value + unit runs** (`21.5°C`, `12°C`, `8.4°C`): the unit is the same size as
  the value or one step smaller (F56+F40, F40+F24) and **shares the baseline**.
  Both are drawn with the M5GFX datum **`baseline_left`** at the same baseline y;
  unit x = value x + value width + 2. There are no ad-hoc offsets. In SVG:
  `dominant-baseline="alphabetic"` at that y (a `<tspan font-size dx="2">` for the
  unit). All other mixed pairs on one line (icon S + F24 text) use the same cy.
- **Baseline of a centred line**: `by = cy + k`, k = 6 (F18), 8 (F24), 14 (F40), 19
  (F56) (0.346 x size, the DejaVu em-box centre). Widget text in SVGs is drawn at
  `by` with `dominant-baseline="alphabetic"`, so it renders the same in every
  viewer; status bar and tab text keep `central`.
- A widget's content block is centred vertically in the widget (centre line
  C = H/2 = 52 for H = 104); see section 4b for the line centres.
- SVG mockups use `font-family="DejaVu Sans"`; status bar and tab text use
  `dominant-baseline="central"` (= the vertical centre datum), widget text uses the
  alphabetic baseline above. Icons are `<use>` squares positioned from their centre.

Line pitch for stacked lines: F24 over F24 = 33 (28.8 box + 4.2 gap); the standard
stacks are in 4b.

Text fitting rule (all widgets): a name is drawn at its fixed size (F24 for widget
names, F18 for sensor and weather 1x1 captions); if it does not fit its box on one
line it is **truncated with "…"**. Names never step down a size and never wrap.
Numbers never truncate; if a value does not fit, drop one decimal, then step down one
font size (F56 to F40 to F24, unit one step below).

## 3. Spacing

| Name | px |
|------|----|
| Screen margin | 16 |
| Gap between widgets, between touch targets | 12 (minimum for any two targets) |
| Widget inner padding | 12 |
| Border (widgets, buttons) | 2, corner radius 8 |
| Focus ring | 3 thick, drawn 3..6 px outside the focused rect (inside the 12 px gap) |
| Status bar height | portrait 56 (incl. 2 px rule); landscape: part of the 64 px top bar |
| Touch target minimum | **64 x 64** |

Ring and dirty-rect inflation: a focused element's redraw region is its rect
inflated by **6 px**. Because gaps are 12, inflated neighbours touch but never
overlap.

### Exceptions to the 64 x 64 rule (chrome only)

The M5Paper is about 234 ppi, so 64 px is about 7 mm, 56 px about 6 mm and 112 px
about 12 mm. Still needed: with 64 px slots the landscape top bar would be 72 high
and the grid would lose 8 px, so the 4 landscape rows would have to shrink to a
102 px cell (and the portrait status bar and tab bar would each grow by 8 px, so
portrait would shrink to 102 too). Keeping 56 px tab chrome is what lets both
orientations close exactly at 104. These are documented, deliberate exceptions,
valid **only** for the elements listed. Every other touch target keeps 64 x 64 and the 12 px gaps.

| Element | Minimum size | Used size |
|---------|--------------|-----------|
| Tab slots and the overflow slot | 112 wide x 56 high | portrait 118 x 56, landscape 120 x 56 |
| Refresh hit area (the "updated" label) | 112 x 56 | portrait 288 x 56, landscape 144 x 56 |
| Settings hit area (battery cluster) | 112 x 56 | 112 x 56 in both orientations |

Consequence, tab chrome only: **the tab focus ring is drawn inside the button**
(section 6) because the tab slots sit 4 px from the top screen edge in landscape.

## 4. Screen areas and grid

Cell height is 104 in both orientations; gap between cells is 12.
Cell pitch = cell size + 12.

| | Portrait 540x960 (default) | Landscape 960x540 |
|---|----------------------------|-------------------|
| Status bar / top bar | status bar (0, 0, 540, 56) | top bar (0, 0, 960, 64), holds status AND tabs |
| Refresh hit area | (0, 0, 288, 56) | (16, 4, 144, 56) |
| Settings hit area (battery cluster) | (412, 0, 112, 56) | (832, 4, 112, 56) |
| Sleep badge region | (298, 3, 100, 48) | (710, 8, 100, 48) |
| Tab bar (redraw region) | bottom: (0, 880, 540, 80) | strip inside the top bar: (172, 0, 648, 64) |
| Content area | (16, 68, 508, 800) | (16, 72, 928, 452) |
| Columns x rows | 2 x 7 | 5 x 4 |
| Cell size w x h | 248 x 104 | 176 x 104 |
| Column x | 16, 276 | 16, 204, 392, 580, 768 |
| Row y | 68, 184, 300, 416, 532, 648, 764 | 72, 188, 304, 420 |
| Pager row (when needed) | row 6: (16, 764, 508, 104) | row 3: (16, 420, 928, 104) |
| Widget rows when pager shown | 6 | 3 |
| "Wide" widget (cover, climate, vacuum) | 2 cells = 508 | 3 cells = 552 |

Derivations (all exact):
- Portrait width: 16 + 248 + 12 + 248 + 16 = 540.
- Portrait height: 56 (status bar) + 12 + (7*104 + 6*12 = 728 + 72 = 800) + 12 +
  80 (tab bar) = 960. Rows end at y = 68 + 800 = 868; tab bar starts 880.
- Landscape width: 16 + 5*176 (= 880) + 4*12 (= 48) + 16 = 960.
- Landscape height: 64 (top bar; slots end at y=60) + 8 + (4*104 + 3*12 = 452) +
  16 = 540. The first row starts at y = 60 + 12 = 72.
- Top bar width (landscape): 16 + 144 (refresh) + 12 + 648 (5*120 + 4*12) + 12 +
  112 (settings hit) + 16 = 960.

### Why these numbers

- **Cell height 104.** The architect target was about 112. With 112 the 452 px
  of landscape height (after the 64 px bar and the 16 px bottom margin) holds
  only 3 rows (360 px) and wastes 92 px; 4 rows need at most 105 per row at gap
  12. 104 is the largest even height that gives 4 landscape rows. In portrait the
  same 104 gives exactly 7 rows: 7*104 + 6*12 = 800 = 960 - 56 - 12 - 12 - 80.
  Both orientations close with no leftover clear space, and one height keeps the
  widget internals (4b) identical in both. A 64 px button inside 104 leaves
  20 px above and below, so buttons never touch the frame.
- **Gap 12** is the new minimum for targets; the grid uses the minimum so cells
  are as large as possible.
- **Margin 16, columns 248 / 176.** `540 - 32 - 12 = 496 = 2*248` and
  `960 - 32 - 48 = 880 = 5*176`; no fractional pixels.
- **Portrait rows: 7, not 6.** 6 rows at 104 would leave 116 px of blank space;
  a 7th row fits exactly.
- **5 landscape columns** (architect decision). Consequence: a 2-cell widget is
  only 364 wide, which cannot hold the cover/climate/vacuum row (cluster of
  216-264 px plus a text block of at least 150 px). Those types are therefore
  "wide" widgets: 2 cells in portrait, **3 cells in landscape** (552). See
  layout.md section 3 and OPEN-1.

A widget of w x h cells has pixel size
`(w*cellW + (w-1)*12) x (h*104 + (h-1)*12)`:

| cells | portrait px | landscape px |
|-------|-------------|--------------|
| 1 wide | 248 | 176 |
| 2 wide | 508 | 364 |
| 3 wide | n/a (clamped to 2) | 552 |
| 4 wide | n/a | 740 |
| 5 wide | n/a | 928 (no type uses it) |
| 1 high | 104 | 104 |
| 2 high | 220 | 220 |
| 3 high | 336 | 336 |
| 4 high | 452 | 452 |

(Every type is 1 high today; the heights exist for the layout engine and its
synthetic tests.)

## 4b. Widget internal grid (shared by every widget)

Widget size is W x 104. All widget specs use these values; coordinates in
widgets.md are widget-relative.

| Name | Value |
|------|-------|
| Frame | (0, 0, W, 104), radius 8, border 2 |
| Padding | 12 on every side: content box (12, 12, W-24, 80) |
| Centre line C | y = 52 |
| Stack A (F24 / F24) | cy = 36 / 69 (baselines 44 / 77), boxes 21.6..50.4 and 54.6..83.4, gap 4.2, total 61.8 |
| Stack B (F40 / F18) | cy = 39 / 78 (baselines 53 / 84), boxes 15..63 and 67.2..88.8, gap 4.2, total 73.8 |
| Toggle stack | name F24 cy 30 (box 15.6..44.4), gap 4.6, line 2 cy 69: icon M (12, 49) + state F24 at x = 58 (total 73.4) |
| Three-line stack | **removed** (does not fit 80 px with real text boxes) |
| Leading icon | M (40) at (12, 49) on the toggle line 2; L (48) at (12, 28), cy = 52 (weather) |
| Right control cluster | buttons 64 x 64 at y = 20 (cy = 52), 12 apart, last one ends at W-12 |
| Badge slot | 24 x 24, normally (W-36, 57): right end of line 2, cy = 69 (exceptions in widgets.md: cover/vacuum, climate, sensor, weather) |

Fill rule (the practical form of "no more than ~25 % empty beyond padding"): the
reserved content boxes of a widget (icon, text column, control cluster) must
span at least 75 % of the inner width (W-24) and at least 60 % of the inner
height (80), centred. Short names leave their own text column partly empty at the
right; that is accepted, the column is still reserved. The numbers per widget
are in widgets.md ("Fill" lines). With revision 4 every widget meets the rule
(the 1x1 toggle is 92 % high).

Button spec (all button rows): **64 x 64**, radius 8, border 2, glyph M (40)
centred. Button row for three: x = W-228, W-152, W-76 (gap 12), y = 20.

## 5. Status elements

Status elements are never touchable except the **refresh label** and the
**battery cluster** (hit areas in section 4). All text F18, INK, centre-line
aligned. The status reflects the last session attempt and stays frozen while
asleep.

### 5.1 Portrait: status bar (0, 0, 540, 56)

White, with a 2 px INK rule at its bottom edge (0, 54, 540, 2). Centre line of
every element: **cy = 27** (the 54 px above the rule).

| Element | Rect | Notes |
|---------|------|-------|
| Connection icon (S, 24x24) | (16, 15, 24, 24) | see 5.3 |
| Refresh label text | x = 48, cy = 27, left, one line, max width 236 (to x = 284) | `updated 14:05` |
| Refresh hit area | (0, 0, 288, 56) | invisible; 10 px from the sleep badge region |
| Sleep badge region | (298, 3, 100, 48) | see 5.5 |
| Battery icon | origin (428, 17), 40 x 20 | see 5.4; cy = 27 |
| Battery percent | right edge x = 524, cy = 27, rect (472, 0, 52, 54) | `78%` |
| Settings hit area | (412, 0, 112, 56) | invisible; battery icon + percent |

### 5.2 Landscape: status part of the top bar (0, 0, 960, 64)

**Rule** (architect decision): full-width 2 px black line (0, 65, 960, 2), centred in the gap between the slots (end y=60) and the grid (start y=72), like the portrait status-bar rule. Static; drawn on every landscape tab frame, only on full redraws (partial updates of the top bar regions, y<=63, never touch it). Centre line of single-line elements: **cy = 32**.

| Element | Rect | Notes |
|---------|------|-------|
| Connection icon (S, 24x24) | (16, 20, 24, 24) | see 5.3 |
| Refresh label text | x = 56, **two lines**, cy = 21 and 43, left, max width 96 | `updated` / `14:05` |
| Refresh hit area | (16, 4, 144, 56) | includes the connection icon |
| Battery icon | origin (848, 22), 40 x 20 | see 5.4; cy = 32 |
| Battery percent | right edge x = 944, cy = 32, rect (892, 4, 52, 56) | `78%` |
| Settings hit area | (832, 4, 112, 56) | invisible; battery icon + percent |

### 5.3 Link states, icons and refresh label (revised 2026-10-29, task 038)

The status bar is never blank: it always shows the link phase. Mockup sheet:
`docs/ui/status-states.svg` (PNG in docs/ui/png/). Icon (S, 24x24, shape not grey) at the
connection-icon rect of 5.1/5.2, label in the label rect. No rect changes.

| State | Icon | Portrait label (one line) | Landscape (two lines) |
|-------|------|---------------------------|-----------------------|
| Connecting Wi-Fi | `wifi-join`: dot + one arc | `connecting Wi-Fi` | `Wi-Fi` / `joining` |
| Fetching states | `cloud-down`: cloud with arrow | `fetching states` | `fetching` / `states` |
| Online | `wifi`: three arcs | `updated 14:05` | `updated` / `14:05` |
| Outdated | link icon as is + `clock` after the label | `updated 14:05` | `updated` / `14:05` |
| Offline (Wi-Fi down or HA unreachable) | `wifi-off`: arcs with a slash (+ clock if values exist) | `offline, last 14:05` | `offline` / `14:05` |
| Auth error | `lock` (+ clock) | `token rejected` | `token` / `rejected` |
| Demo | `demo`: filled circle with play triangle | `demo mode` | `demo` / `mode` |
| Never fetched, cache empty | `wifi` (or the state above) | `not updated yet` | `updated` / `never` |

Earlier day: `updated 26.10. 14:05` (landscape `26.10.` / `14:05`); if the portrait text is wider than
206 px drop the word `updated`. Time = last successful fetch from the RTC. Manual refresh and
every wake show `fetching states` until the fetch ends (then online, outdated, offline or auth
error). Values in widgets are never blanked or greyed because of staleness.

**Outdated marker** replaces the old black stale chip: a small `clock` icon (S, INK, no chip).
Shown when the last good fetch is older than 2 x refresh_interval (3 h when refresh is off) or the
last fetch failed, and always together with offline / auth error when cached values are shown.
Position: portrait (260, 15, 24, 24) (right edge x = 284, label text max 206 wide, ends at most
x = 254); landscape (136, 20, 24, 24) (inside the refresh hit area, ends x = 160; label lines end
at most x = 130; the tab strip starts x = 172).

**Redraw:** each phase change repaints only the connection icon + label + clock region (portrait
(0, 0, 288, 54), landscape (16, 4, 144, 56)) with the fast partial mode, so a phase change is a
small update. Battery and sleep regions are separate. A full refresh is never needed for it.

**The label is a button: a tap refreshes now.** Feedback: icon and label switch to `fetching
states` until the fetch ends; further taps while a fetch runs are ignored. Rocker has no
equivalent (long push opens Settings).

OPEN: date/time format (24 h, d.m.) as a setting later.

### 5.4 Battery icon

Drawn from explicit rectangles (no strokes), so mockup and spec agree
pixel for pixel. Origin (bx, by) = portrait (428, 17), landscape (848, 22).

| Part | Rect relative to (bx, by) | Fill |
|------|---------------------------|------|
| Body outer | (0, 0, 36, 20) | INK |
| Body inner | (2, 2, 32, 16) | PAPER |
| Level fill | (2, 2, fw, 16), `fw = round(32 * pct / 100)` | INK |
| Nub | (36, 5, 4, 10) | INK |

Portrait: body (428,17,36,20), inner (430,19,32,16), nub (464,22,4,10).
Landscape: body (848,22,36,20), inner (850,24,32,16), nub (884,27,4,10).
At 78 %: fw = 25.

At <= 15 % the level fill is replaced by an "!" glyph; charging draws a bolt in
the body. The percent text is always shown. (Critical-battery screen: system
screen, not specified here.)

**Tap on the battery cluster opens Settings** (hit kind `settings`, layout.md
section 6). This is the touch path to Settings; the rocker path is a long push.
No feedback on the cluster itself (the screen changes with a full refresh).

### 5.5 Sleep badge

Purpose: the e-ink image is frozen while the device sleeps and looks awake. Right before every
power-off or deep sleep the firmware draws **only the badge region** (partial refresh) so the user
sees "asleep, not usable". One badge for every wake mode (Touch, Button, Both) and for low-battery
sleep; no wake text or glyph (the battery cluster already shows low battery). 1-bit INK/PAPER only.

**Drawing primitives (identical in both orientations, only the position differs)**
1. Badge: filled rounded rect 96 x 44, radius 12, INK (grey 0), no border.
2. Label: `Zz`, DejaVu Sans **Bold** 30 px, fill PAPER (grey 15), horizontally centred on the badge
   centre x, vertically centred (middle baseline) at badge cy + 1.
3. Nothing else. No icons.

| | Portrait | Landscape |
|---|---|---|
| Badge rect | (300, 5, 96, 44) | (712, 10, 96, 44) |
| Centre (cx, cy) | (348, 27) | (760, 32) |
| Label anchor | x = 348, cy = 28 | x = 760, cy = 33 |
| Redraw region (badge + 2 px) | (298, 3, 100, 48) | (710, 8, 100, 48) |
| Where | free gap between the status label (clock icon ends x = 284 at most) and the battery cluster (starts 412) | the **last tab slot** (700, 4, 120, 56), badge centred in it |

Landscape: only the last slot is covered; the other tabs stay visible (the old design hid the
whole tab strip). With 5 tabs the slot shows tab 5 (or the overflow slot) and is hidden while
asleep.

Rules:
- Drawn **last**, after any ghost-clearing full refresh that is due before sleep (section 10),
  then the EPD is powered off. If the draw fails the device sleeps without it (no retry).
- **Cleared on wake**: the first draw after any wake redraws the redraw region from the normal
  layout (portrait: PAPER fill; landscape: the last tab slot with its normal state, active or
  not), partial refresh, region only. Being small (100 x 48) it can be the first thing drawn,
  before Wi-Fi/fetch (task 058). A scheduled (RTC) wake that changes nothing draws nothing: the
  badge stays.
- The tap that wakes the device (wake mode Touch) is consumed as the wake event and not delivered
  to the tab strip or widgets (OPEN-3 in layout.md).
- Not a hit target. It never overlaps another hit area (portrait: refresh hit ends x = 288, badge
  region 298..398, settings hit starts 412; landscape: region inside slot 5, x 710..810).
- Not shown while the Settings screen is open; Settings auto-leaves to the tab screen before
  sleeping (settings.md rule 5), then the badge is drawn.
- The sleep plan no longer selects a variant; the `touch_wake` flag does not affect the badge.

## 6. Tab bar

Tab slot = button, 56 high, 2 px border, radius 8, F18 label centred (centre
line cy = slot centre), up to 2 lines (word wrap, ellipsis on the 2nd line; line
centres at slot cy -11 and +11), label width = slot width - 12.
Inactive: PAPER fill, INK border. **Active: INK fill, PAPER label** (solid
black against outline: distinguishable by shape and fill, not by a grey).

| | Portrait | Landscape |
|---|----------|-----------|
| Position | bottom bar | inside the top bar, between refresh label and battery |
| Bar rect (redraw region) | (0, 880, 540, 80) | (172, 0, 648, 64) |
| Separator | 2 px INK rule (0, 880, 540, 2) | none |
| Slots | 4 | 5 |
| Slot rect i | (16 + i*130, 892, 118, 56) | (172 + i*132, 4, 120, 56) |
| Slot x | 16, 146, 276, 406 | 172, 304, 436, 568, 700 |
| Slot y, centre line | 892, cy = 920 (lines 909 / 931) | 4, cy = 32 (lines 21 / 43) |
| Label width | 106 | 108 |
| Tabs per group when overflowing | 3 | 4 |
| Overflow slot | slot 3 | slot 4 |

Checks: portrait 16 + 4*118 + 3*12 + 16 = 540; vertical 880 (rule) + 2 + 10 +
56 (slots 892..948) + 12 = 960. Landscape: slots 172..820 (5*120 + 4*12 = 648),
12 px to the refresh hit area (ends x=160) and 12 px to the settings hit area
(starts x=832).

Overflow (more tabs than slots, max 16 tabs): tabs are shown in groups. If
tab count <= slots, all tabs show and there is no overflow slot. Otherwise
the last slot is the overflow button: outline, `dots` icon (S) at slot-relative
(28, 16, 24, 24) and the text `g/G` (F18, current group / group count) left at
slot-relative x = 60, cy = 28. Tapping it shows the next group of tabs (wraps to
group 1). It changes only the tab bar's buttons, not the active tab. The active
tab is always in the group shown when it becomes active. Empty slots in a short
last group are left blank (no outline).

Capacity: portrait shows 4 tabs, or 3 + overflow (max 16 tabs = 6 groups);
landscape shows 5 tabs, or 4 + overflow (max 16 tabs = 4 groups). A fifth portrait
slot would need 5*112 + 4*12 = 608 > 508; a sixth landscape slot 6*112 + 5*12 =
732 > 648.

**Tab focus (rocker, TAB mode):** because the focus in TAB mode is always the
active (INK) tab, the focus is shown as a 2 px PAPER frame drawn inside the
button, inset 4 px (rect `(x+4, y+4, w-8, h-8)`, radius 4). The redraw region
is the bar rect.

## 7. Sub-page indicator ("pager")

Shown only when the active tab has more than one sub-page. It occupies the
whole last grid row (section 4) and is part of the content area (the layout engine
removes that row from widget placement; see layout.md). Buttons are 120 x 64,
vertically centred in the 104 px row (top = row y + 20).

| Element | Portrait rect | Landscape rect |
|---------|---------------|----------------|
| Pager rect | (16, 764, 508, 104) | (16, 420, 928, 104) |
| Previous button (120x64) | (16, 784, 120, 64) | (16, 440, 120, 64) |
| Next button (120x64) | (404, 784, 120, 64) | (824, 440, 120, 64) |
| Label `2 / 3` (F40), centred | (148, 784, 244, 64), cy = 816 | (148, 440, 664, 64), cy = 472 |

Buttons use the chevron icons (M, 40) centred. At the first sub-page the previous
button is unavailable-styled (dotted OFF border, no action); likewise next on the
last (no wrap by touch; the rocker wraps, see layout.md).

## 8. Icons

Line/solid glyphs on a 24x24 design grid, drawn as vector paths, stroke 2
(scales with size). Sizes: **S = 24** (status bar, badges, inline in text
lines), **M = 40** (buttons, leading icons of 1x1 toggles, sleep badge),
**L = 48** (weather condition icon, 1x1 and 2x1). XL (64) is retired: it left no room
for the temperature and humidity on one row.

| Group | Icons |
|-------|-------|
| Cover | up (solid triangle), stop (solid square), down |
| Devices | bulb (solid, on), bulb_off (outline), plug (switch), auto (two circular arrows, automation), vacuum (disc with bumper), thermometer, gauge (generic sensor), bolt |
| Binary sensor | window_on (window outline, panes filled), window_off (outline, panes empty); per device class pairs _on (solid) / _off (outline): door, garage, motion, person, puddle, smoke, gas, problem, battery_low, lock_open / lock_closed, plug_off, bolt_off (widgets.md 11.1); fallback dot_on / dot_off (ring) |
| Climate | flame (heating: two tongues and a hollow, never drop-shaped), snow (cooling), dry (drop with a slash, drying), fan, auto, power (off), pause (two bars, hvac action idle) |
| Humidity | drop (solid drop, used **only** for humidity) |
| Vacuum | play (start), pause, dock (house with down arrow) |
| Controls | minus, plus, chevron_left, chevron_right, dots (overflow, three dots), refresh (open circular arrow with a head) |
| System | wifi, wifi_dot, wifi_off, battery, clock, hourglass, warn (circle with slash = unavailable), alert (circle with "!" = failed) |
| Sleep | moon (crescent), sidebtn (bar with an arrow pointing at it), finger (tap symbol: filled dot with two concentric ripple rings), moon is a thin stroke-like crescent |
| Weather (XL / L) | sunny, clear_night, partlycloudy, cloudy, rainy, pouring, snowy, fog, windy, lightning, unknown |

Climate icon use: the hvac action icon on the climate line 2 is `flame` for heating,
`snow` for cooling, `dry` for drying, `fan` for fan, `pause` for idle; when no
action is known the mode icon is used (`flame` heat, `snow` cool, `auto`
heat_cool/auto, `power` off, `fan` fan_only, `dry` dry). `drop` never stands for an
action.

Weather state to icon: `sunny`, `clear-night`, `partlycloudy`, `cloudy`,
`rainy`, `pouring`, `snowy`/`snowy-rainy`, `fog`, `windy`/`windy-variant`,
`lightning`/`lightning-rainy`, anything else `unknown`.

## 9. Visual states

Applies to every widget; widget-specific details are in widgets.md.

| State | Look (shape first) | Touch |
|-------|--------------------|-------|
| Off / idle | PAPER fill, 2 px solid INK border, INK content | active |
| On / active | **INK fill**, PAPER content (toggles); for button rows: the button of the current mode is INK-filled | active |
| Pending (command queued or sent, not yet confirmed) | the **requested** result is drawn (e.g. toggle flips to its new fill), border becomes **3 px dashed** (8 on / 6 off), an hourglass (S) replaces the widget badge (toggles, climate); for button rows (cover, vacuum) the tapped button gets the dashed border and **one** hourglass (M) instead of its glyph, and there is no badge | active (a further tap on the same control while pending is ignored; on other controls it is queued) |
| Failed (send failed or state not confirmed after the next fetch) | pending is dropped, the real state is drawn, alert (S) badge stays until the next successful fetch | active |
| Unavailable (`unavailable`, `unknown`, missing entity) | PAPER fill, **2 px dotted** (2 on / 4 off) OFF border, static title and icons in OFF grey, dynamic text (`n/a` or `-`) INK, warn (S) badge, buttons dotted | none (taps ignored, no redraw) |
| Loading (no cached value yet) | normal frame, values `-` in INK, no badge | controls active |
| Stale (global: status-bar clock icon is shown) | no change to the widget (values stay INK); only the status-bar clock icon marks staleness | active |
| Focused (rocker) | 3 px INK ring 3..6 px outside the rect; inside a multi-button widget the active button also gets a 4 px solid border. **Tab slots:** inner frame instead (section 6) | n/a |
| Read-only widgets (sensor, binary_sensor, weather) | no pending state, never focusable | no feedback |

Every widget has one **badge slot** (24 x 24). It shows hourglass (pending),
alert (failed), warn (unavailable) or nothing; it is a distinct dirty region.
Its position per widget type is in widgets.md (default (W-36, 57)).

Mockup ids (for `tools/check_mockups.py`): tab mockups give each widget
`<g id="w-N">` with the border `<rect id="w-N-frame">`; every text and icon inside
has an id starting `w-N-`; button outlines end in `-btn`. No element may leave the
frame minus 12 px, and no two elements of a widget may overlap.

## 10. Refresh rules

The firmware owns the mechanics; the UI requires the following (ARCHITECTURE.md
2026-10-28, user-approved on the device).

**Feedback refresh (a tap, a changed value, a focus move; only the listed rect):**
- Mode: `epd_fastest` (about 130 ms) **only where the region's current content is
  pure black/white**. Almost everything qualifies because dynamic text is INK.
- A region that currently holds greys (for example right after a GC16 full
  refresh drew DIM captions, OFF hairlines or dotted frames inside it) gets
  **one `epd_quality` refresh** instead. Never fastest over greys: old text stays
  visible for 1 to 2 taps.
- **No per-tap clean-up pass.** A tap produces exactly one partial refresh of its
  region(s), no follow-up pass 1 to 5 s later.
- Regions: tap feedback = the control rect (toggle: the whole widget; button row:
  that button; climate +/-: the tapped button plus the target value rect and the
  badge); a value changed by a fetch = the smallest element rect that changed
  (merged into the widget rect if the changed rects cover more than 60 % of it);
  focus ring moves = old and new element rects, each inflated by 6 px; status
  elements = the changed element only (a tap on the refresh label redraws the
  connection icon and, when the fetch ends, the label); rocker tab
  highlight = the bar rect; **sleep indicator: draw and clear touch only its
  region (section 5.5).**
- Regions are rounded out to 4 px by the driver; the 12 px gaps absorb that
  (landscape: the tab strip rect ends at y = 64 and the first widget row starts
  at y = 72; portrait: status bar ends y = 56, grid starts 68; tab bar starts
  y = 880, grid ends 868).

**GC16 full refresh (whole screen, 16 greys, clears ghosting, flashes):**
- On every tab change, sub-page change or orientation change (the content is new).
- Settings screen enter/exit (including the orientation change applied on exit).
- A scheduled (RTC) refresh that changed at least one widget, and a fetch that
  changes more than 6 widgets at once.
- **Ghost debt:** after 12 feedback refreshes since the last full refresh, or when
  the summed partial area exceeds 150 % of the screen, one runs at the next quiet
  moment (no input for 2 s); at the latest just before sleeping, **before** the
  sleep indicator is drawn. OPEN: tune 12 / 150 % on the device.
- Never refresh when nothing changed. Never redraw on wake before first input or
  data (the screen already shows the last frame), **except** the one partial draw
  that clears the sleep indicator.

Consequence for the mockups: right after a full refresh the screen holds greys
(captions, hairlines), so the first tap in a region that contains them is one
quality refresh; later taps in the same region are fastest.

**Rocker tab switching** debounce: the tab bar highlight moves immediately
(partial); the content GC16 full refresh runs after 400 ms without further
presses.

## 10b. Rocker yes/no rule (every dialog)

Any yes/no dialog or confirmation uses the rocker as follows: **UP = yes, DOWN =
no, PUSH = skip** (dismiss without a decision). PUSH is never "yes". The dialog
shows the three answers as labelled targets (yes / no / skip), each at least
64 x 64 for touch.
