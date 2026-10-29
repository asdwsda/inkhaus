# Layout engine spec

Exact spec for `lib/ui` (tasks 006, revised by 011, revised again for the compact
geometry). Values come from `docs/ui/STYLE.md`; widget internals and sub-region
rectangles are in `docs/ui/widgets.md`; the Settings screen is in
`docs/ui/settings.md`. Rect = (x, y, w, h). A point is inside a rect when
`x <= px < x+w` and `y <= py < y+h`.

Revision 3 (compact): cell 104 high, gap 12, touch minimum 64, portrait 2 x 7,
landscape 5 x 4, "wide" widgets (2 cells portrait, 3 landscape), all types are
1 row high, new hit kind `settings`, new rect `sleepIndicator`, focus ring and
dirty inflation 6.

## 1. Inputs and outputs

Input: orientation (`portrait` | `landscape`), tab list (title + widgets with
type and optional `size`), active tab index, active sub-page index, and the
visible tab group (derived, see 5).

Output `Layout`:
- `statusBar` rect (portrait (0,0,540,56); landscape the whole top bar
  (0,0,960,64)), `tabBar` rect (redraw region of the tab slots), `refreshHit`
  and `settingsHit` rects (the two touchable status areas), `sleepIndicator`
  rect (STYLE.md 5.5; the firmware draws it, the engine only supplies the rect);
- `tabSlots[]`: slot rects, each with `{kind: tab|overflow|empty, tabIndex}`;
- `pager`: rect, prev/next button rects, label rect, `visible` flag, `subPage`,
  `subPageCount`;
- `widgets[]` for the active sub-page: `{index (in tab), type, rect,
  cellX, cellY, cellW, cellH, clamped}`;
- `clampReports[]`: widgets whose requested size was changed.

Note (task 038): no rect changes. The stale chip is gone (clock icon instead, STYLE.md 5.3); the
status label redraw region equals `refreshHit`; the sleep indicator is an outline pill inside the
same `sleepIndicator` rect.

## 2. Grid geometry

| | Portrait | Landscape |
|---|----------|-----------|
| Screen | 540 x 960 | 960 x 540 |
| Status bar | (0,0,540,56), rule (0,54,540,2) | top bar (0,0,960,64), status and tabs share it |
| Refresh hit area | (0,0,288,56) | (16,4,144,56) |
| Settings hit area | (412,0,112,56) | (832,4,112,56) |
| Sleep indicator rect | (292,3,112,48) | (172,4,648,56) |
| Tab bar (redraw region) | (0,880,540,80) | (172,0,648,64) |
| Content area | (16,68,508,800) | (16,72,928,452) |
| Columns / rows | 2 / 7 | 5 / 4 |
| Cell | 248 x 104 | 176 x 104 |
| Gap | 12 | 12 |
| Column x | 16, 276 | 16, 204, 392, 580, 768 |
| Row y | 68, 184, 300, 416, 532, 648, 764 | 72, 188, 304, 420 |

Cell (c, r) origin: `x = colX[c]`, `y = rowY[r]`. A widget of `w x h` cells at
(c, r) has rect `(colX[c], rowY[r], w*cellW + (w-1)*12, h*104 + (h-1)*12)`.
All rects end inside the content area: portrait max right edge 524, bottom 868;
landscape right 944, bottom 524.

Arithmetic (all exact):

| Check | Sum |
|-------|-----|
| Portrait width | 16 + 248 + 12 + 248 + 16 = 540 |
| Portrait height | 56 + 12 + (7*104 + 6*12 = 728 + 72 = 800) + 12 + 80 = 960 (grid 68..868, tab bar 880..960) |
| Landscape width | 16 + (5*176 + 4*12 = 880 + 48 = 928) + 16 = 960 |
| Landscape height | 72 + (4*104 + 3*12 = 416 + 36 = 452) = 524; + 16 = 540 |
| Landscape top bar | 4 + 56 (slots) + 4 = 64; slots end y=60; 60 + 12 = 72 = first row |
| Landscape top bar, horizontal | 16 + 144 + 12 + 648 + 12 + 112 + 16 = 960 |
| Portrait tab slots | 16 + 4*118 + 3*12 + 16 = 540 |
| Portrait gaps to targets | status bar ends y=56, grid starts y=68: 12; grid ends y=868, tab bar rule y=880: 12 |

Widget pixel sizes: STYLE.md section 4.

## 3. Widget sizes

Size in cells (w x h). "Wide" = 2 cells in portrait, 3 cells in landscape
(the 364 px two-cell width of the 5 landscape columns cannot hold the row layouts
of widgets.md; see OPEN-1). Every type is exactly 1 row high; a taller request is
clamped. "Default" is used when config has no `size`.

| Widget type | Portrait: default (min..max) | Landscape: default (min..max) | Notes |
|-------------|------------------------------|-------------------------------|-------|
| cover | 2x1 (2x1..2x1) | 3x1 (3x1..3x1) | wide |
| cover group (`group:` entry of covers) | 2x1 | 3x1 | same layout as cover |
| light | 1x1 (1x1..1x1) | 1x1 (1x1..1x1) | |
| switch | 1x1 | 1x1 | |
| automation | 1x1 | 1x1 | |
| climate | 2x1 (2x1..2x1) | 3x1 (3x1..3x1) | wide |
| sensor | 1x1 (1x1..2x1) | 1x1 (1x1..2x1) | 2x1 = name left, value right |
| binary_sensor | 1x1 (1x1..1x1) | 1x1 | |
| weather | 2x1 (1x1..2x1) | 2x1 (1x1..2x1) | |
| vacuum | 2x1 (2x1..2x1) | 3x1 (3x1..3x1) | wide |

Why fixed sizes: a 1x1 toggle stretched to 2 cells would leave a large empty area
(STYLE.md 4b fill rule), and the wide widgets only have one layout. A config
`size` that differs is clamped and reported.

Groups: a `group:` entry has the widget type of its members' domain (all
members share one domain). Only `cover` groups are required in v1; light and
switch groups use the light/switch type.

Clamp rule (applied per widget, in this order):
1. `w = clamp(w, min.w, max.w)`; `h = clamp(h, min.h, max.h)` for the type and
   the orientation (table above).
2. `w = min(w, columns)`.
3. `h = min(h, rows - 1)` (portrait: 6, landscape: 3). **Safeguard only:** after
   step 1 the largest height is 1, so with today's type table step 3 never changes
   anything. It stays so that a future taller widget type still fits when the
   pager row is reserved. It can only be exercised with synthetic type tables in
   tests.

If the result differs from the requested size (config size or default), report
it in `clampReports` (widget index, requested, used). Type minimums never
exceed the grid.

## 4. Flow algorithm and sub-pages

```
place(widgets, rows):                       # rows = 7/4 (no pager) or 6/3 (pager)
  pages = [ emptyOccupancy(columns, rows) ]
  for each widget in config order (after clamp):
     cell = firstFit(pages.last, widget.w, widget.h)
     if cell is none:
        pages.append(emptyOccupancy(columns, rows))
        cell = firstFit(pages.last, widget.w, widget.h)   # always fits (clamped)
     mark cells occupied; widget.page = pages.size-1; widget.cell = cell
  return pages

firstFit(occ, w, h): for r in 0..rows-h: for c in 0..columns-w:
                        if all cells of (c..c+w-1, r..r+h-1) free: return (c, r)
```

- Row-major, first fit, within one sub-page only: a later small widget fills
  earlier gaps of the *current* sub-page, but never goes back to a finished
  earlier sub-page. Config order decides which sub-page a widget lands on.
- Two passes: first `place(widgets, rows_full)`. If everything fits on one
  page, that is the result (no pager). Otherwise run `place(widgets,
  rows_full - 1)`; the pager occupies the freed last row. (A first-fit result
  that needs 2+ pages with `rows_full` always needs 2+ with fewer rows.)
- Sub-page count = `pages.size`. Active sub-page is clamped to
  `[0, count-1]`; changing tab resets it to 0.
- Empty tab (no widgets): one page, empty content area, no pager.

Worked example ("Living room" tab of config.example.yaml, plus a sensor, an
automation and a weather appended so every layout is visible). Config order:
cover (wide), group (wide), light Ceiling, light Floor lamp, switch Fan, climate
(wide), binary_sensor Window, sensor Outside, automation Night mode, weather (2x1).

| # | Widget | Portrait cell (c,r) | Portrait rect | Landscape cell (c,r) | Landscape rect |
|---|--------|---------------------|---------------|----------------------|----------------|
| 1 | cover | (0,0) | (16,68,508,104) | (0,0) | (16,72,552,104) |
| 2 | group | (0,1) | (16,184,508,104) | (0,1): row 0 has only cols 3-4 free, too narrow for 3 | (16,188,552,104) |
| 3 | light Ceiling | (0,2) | (16,300,248,104) | (3,0) | (580,72,176,104) |
| 4 | light Floor lamp | (1,2) | (276,300,248,104) | (4,0) | (768,72,176,104) |
| 5 | switch Fan | (0,3) | (16,416,248,104) | (3,1) | (580,188,176,104) |
| 6 | climate | (0,4): col 1 of row 3 is too narrow | (16,532,508,104) | (0,2): rows 0 and 1 have no free 3-wide run | (16,304,552,104) |
| 7 | binary_sensor Window | (1,3): first fit fills the gap beside the switch | (276,416,248,104) | (4,1) | (768,188,176,104) |
| 8 | sensor Outside | (0,5) | (16,648,248,104) | (3,2) | (580,304,176,104) |
| 9 | automation Night mode | (1,5) | (276,648,248,104) | (4,2) | (768,304,176,104) |
| 10 | weather | (0,6) | (16,764,508,104) | (0,3) | (16,420,364,104) |

Everything fits in 7 / 4 rows: no pager. Rects: `docs/ui/tab-portrait.svg` /
`tab-landscape.svg`.

## 5. Tab bar and status elements

Rects: STYLE.md sections 5 and 6. Slot count: portrait 4, landscape 5.

Visible tab group:
- `T` = tab count. If `T <= slots`: group is always "all", slot i shows tab i,
  remaining slots are `empty`, no overflow slot.
- Else: `per = slots - 1` tabs per group (3 portrait, 4 landscape), group count
  `G = ceil(T / per)`, overflow slot = last slot. Slot i (i < per) shows tab
  `group*per + i` if it exists, else `empty`.
- `group` is a view state: it is set to `floor(activeTab / per)` whenever the
  active tab changes (any source), and advanced by the overflow button
  `(group + 1) mod G` without changing the active tab.
- Max 16 tabs: portrait G <= 6, landscape G <= 4.

## 6. Hit-test regions

`hitTest(point) -> {kind, index, sub}` checked in this order; rects from the
Layout; gaps and margins and the rest of the status elements return `none`.

| Kind | Region | index | sub |
|------|--------|-------|-----|
| `tab` | a `tab` slot rect | tab index | 0 |
| `tabOverflow` | the overflow slot rect | 0 | 0 |
| `refresh` | `refreshHit`: portrait (0,0,288,56), landscape (16,4,144,56) | 0 | 0 |
| `settings` | `settingsHit` (the battery cluster): portrait (412,0,112,56), landscape (832,4,112,56) | 0 | 0 |
| `pagerPrev` / `pagerNext` | pager button rects (only if the pager is visible) | 0 | 0 |
| `widget` | sub-region rect (see widgets.md) of a widget on the active sub-page | widget index in tab | sub-region id (>= 1) |
| `widget` | any other point inside the widget rect | widget index | 0 |
| `none` | everything else | | |

`refresh` is a tap target: the app starts a state fetch (STYLE.md 5.3).
`settings` is a tap target: the app opens the Settings screen (settings.md; the
same effect as the rocker LONG_PUSH in TAB mode, including the full refresh).
The `refreshHit` and `settingsHit` rects never overlap a tab slot or each other
(landscape: refresh ends x=160, first slot starts 172, last slot ends 820,
settings starts 832; portrait: refresh ends x=288, settings starts 412) or a
widget (portrait: both end y=56, grid starts y=68; landscape: both end y=60, grid
starts y=72). The sleep indicator region is not a hit target (it exists only
while asleep).

Sub-region id 0 means "widget body". Widgets with a single action (toggles)
have no sub-regions: the whole rect is id 0 and triggers the action. For
multi-button widgets, id 0 triggers nothing (the app ignores it). Sub-region
rects never overlap and lie 12 px apart (button rows) so rect edges are the
hit boundary; no extra slop. The layout engine therefore also exports
`subRegions(type, size, widgetRect) -> [{id, rect}]` using the formulas in
widgets.md (all buttons 64 x 64, y = 20).

Unavailable widgets: the engine still returns hits; the app ignores them.

## 7. Rocker

### 7.1 Physical to logical mapping

Assumption (no M5Paper docs in the repo; to verify on hardware): the 3-way
rocker sits on the device's right edge when held portrait. Rotating the device
90 degrees counter-clockwise to landscape moves it to the top edge, which is
the case the user described. Physical keys: `UP` = the end towards the
portrait top, `DOWN` = the end towards the portrait bottom, `PUSH`.
(M5Unified: BtnA / BtnB / BtnC; to be confirmed, expected A = UP, B = PUSH,
C = DOWN.)

Logical keys: `PREV`, `NEXT`, `PUSH`, `LONG_PUSH` (>= 600 ms hold).

| Orientation | UP | DOWN | Spatial meaning for the user |
|-------------|----|------|------------------------------|
| portrait | PREV | NEXT | top end = previous, bottom end = next |
| landscape (rocker on top edge) | PREV | NEXT | UP end points to screen-left: left end = previous, right end = next |

So the mapping is the same physical-to-logical in both orientations; what
changes is the direction it points on screen (vertical in portrait, horizontal
in landscape), which matches the flow of both layouts (top-to-bottom, left-to-
right). The engine exposes `mapRocker(physicalKey, orientation, invert)`;
`invert` swaps PREV and NEXT. OPEN: if the device turns out to be rotated
the other way in landscape (UP end towards screen-right), set `invert=true`
for landscape. Also OPEN: a settings flag for it.

### 7.2 Modes and focus model

State: `mode` in {TAB, WIDGET, INNER}, `focus`. On wake `mode = TAB`, no ring
is drawn until the first rocker event (the frozen screen stays clean).

**First event after wake.** The first rocker event after wake **both reveals
the ring and performs its action**; it is not swallowed to "just show the
focus". Examples: NEXT in TAB mode switches to the next tab and the (inner)
tab frame appears on it; PUSH enters WIDGET mode and the ring appears on the
first focusable widget; LONG_PUSH opens Settings (no ring). The same applies to
the first event after a touch-only period: the ring is drawn as part of the
result of the action. (The sleep indicator is cleared before the event is
handled, STYLE.md 5.5; in landscape this restores the tab strip, so the tab
frame can be drawn.)

**TAB mode** (default). The focus is the active tab; its button shows the
tab focus frame (STYLE.md section 6).
- PREV / NEXT: previous / next tab, wrapping (last -> first). The tab bar
  highlight updates at once (partial); the content switches after 400 ms of no
  presses (full refresh). The visible tab group follows (section 5).
- PUSH: enter WIDGET mode on the first focusable widget of the active (kept)
  sub-page. If there is none, no effect.
- **LONG_PUSH: open the Settings screen** (docs/ui/settings.md). Refreshing now is
  a tap on the status bar's "updated" label (hit kind `refresh`) and a button
  inside Settings. Touch path to Settings: a tap on the battery cluster (hit kind
  `settings`).

**Focusable widgets**: cover, cover group, light, switch, automation, climate,
vacuum, and only if they are not unavailable. Sensor, binary_sensor and
weather are read-only and skipped.

**WIDGET mode**. Focus stops are, in order: stop 0 = the tab bar (leaves to TAB
mode), then the focusable widgets of the active sub-page sorted by (row, column)
of their top-left cell (stops 1..n). The ring is drawn around the focused widget
rect.
- NEXT on stop k: stop k+1. After the last widget: if a later sub-page with at
  least one focusable widget exists, go to that sub-page (full refresh), stop 1.
  Otherwise this is the **end**: go to stop 0, i.e. **TAB mode with the ring
  (frame) on the active tab, and the active sub-page is kept** (it is not reset
  to 0; the tab did not change, so the sub-page does not change either). A
  following PUSH in TAB mode re-enters WIDGET mode at the first focusable widget
  of that kept sub-page.
- PREV on stop k: stop k-1. Before the first widget: if an earlier sub-page with
  at least one focusable widget exists, go to it (full refresh), last stop.
  Otherwise this is the **end**: go to stop 0 (TAB mode), sub-page kept.
- **"No further sub-page in a direction has a focusable widget" counts as the
  end** for that direction, exactly as if the current sub-page were the last
  (NEXT) or first (PREV) one. Sub-pages without a focusable widget (only
  sensors, weather, or unavailable widgets) are skipped, and if all sub-pages
  beyond the current one in that direction are such, no sub-page change happens.
- PUSH on a single-action widget (light, switch, automation): run its action.
  PUSH on a multi-button widget (cover, group, climate, vacuum): enter INNER
  mode.
- LONG_PUSH: back to TAB mode.

**INNER mode** (multi-button widgets). The widget ring stays; the active button
gets a 4 px solid border. Order of stops (from widgets.md): cover/group
up, stop, down (entry = stop); climate minus, plus (entry = minus; the mode is
display-only and is not a stop); vacuum start, pause, dock (entry = start).
- PREV / NEXT: previous / next button, wrapping.
- PUSH: run the button's action (stay in INNER). For climate minus/plus, repeated
  pushes accumulate in the pending target before it is sent.
- LONG_PUSH: back to WIDGET mode (same widget).
- Disabled buttons (unavailable widget) are skipped.

**Touch interplay**: a touch tap acts at once and does not change the rocker
mode; the first rocker event after a touch continues from the last rocker
focus, except that a tap on a tab sets TAB mode with that tab. Tabs by touch
are always active in every mode.

**Pager by touch**: `pagerPrev`/`pagerNext` change the sub-page (full refresh,
focus returns to TAB mode). Not wrapping.

**Settings** has its own rocker model (settings.md). Leaving Settings returns to
the tab screen in TAB mode with the same active tab and sub-page (sub-page
clamped if the orientation changed), without a ring.

### 7.3 Redraws caused by focus

Ring move: redraw old focus rect and new focus rect, each inflated by 6 px
(partial). Tab/sub-page change: full refresh. Mode change TAB <-> WIDGET:
ring (and tab frame) moves only; the tab frame redraw region is the tab bar
rect.

## 8. Engine test vectors

Marked (changed) = existed before with other numbers; (new) = new vector.
Everything in this section replaces the previous section 8; the firmware tests
are generated from it.

### 8.1 Geometry

- (changed) Portrait slot rects: (16,892,118,56), (146,892,118,56),
  (276,892,118,56), (406,892,118,56).
- (changed) Landscape slot rects: (172,4,120,56), (304,4,120,56),
  (436,4,120,56), (568,4,120,56), (700,4,120,56).
- (changed) Status/refresh/settings/tab bar rects. Portrait: statusBar
  (0,0,540,56), refreshHit (0,0,288,56), settingsHit (412,0,112,56), tabBar
  (0,880,540,80), content (16,68,508,800). Landscape: statusBar (0,0,960,64),
  refreshHit (16,4,144,56), settingsHit (832,4,112,56), tabBar (172,0,648,64),
  content (16,72,928,452).
- (new) `sleepIndicator` rect: portrait (292,3,112,48); landscape (172,4,648,56).
- (changed) Columns x: portrait 16, 276; landscape 16, 204, 392, 580, 768.
  Rows y: portrait 68, 184, 300, 416, 532, 648, 764; landscape 72, 188, 304, 420.
  Columns / rows: portrait 2 / 7, landscape 5 / 4.
- (changed) 1x1 portrait cell (1,0) = (276,68,248,104); 2x1 portrait at (0,1) =
  (16,184,508,104).
- (new, synthetic geometry; no real type is taller than 1 row) 1x2 portrait at
  (0,2) = (16,300,248,220); 2x3 portrait at (0,4) = (16,532,508,336) (bottom edge
  868); 1x4 landscape at (0,0) = (16,72,176,452) (bottom edge 524).
- (changed) 3x1 landscape at (0,0) = (16,72,552,104); 1x1 landscape at (4,0) =
  (768,72,176,104); 2x1 landscape at (2,3) = (392,420,364,104); 4x1 landscape at
  (1,2) = (204,304,740,104) (right edge 944); 5x1 landscape at (0,0) =
  (16,72,928,104).
- (changed) Pager rects. Portrait: pager (16,764,508,104), prev (16,784,120,64),
  next (404,784,120,64), label (148,784,244,64). Landscape: pager
  (16,420,928,104), prev (16,440,120,64), next (824,440,120,64), label
  (148,440,664,64).
- (changed) Widget rows when the pager is shown: portrait 6 (rows 0..5), landscape
  3 (rows 0..2); pager row index 6 / 3.
- (new) Closing sums: portrait 16+248+12+248+16 = 540 and 56+12+800+12+80 = 960;
  landscape 16+880+48+16 = 960 and 72+452+16 = 540. A unit test asserts them from
  the constants.

### 8.2 Sizes, clamping and flow

- (changed) Type table, portrait / landscape (default = min for fixed types):
  cover 2x1 / 3x1; cover group 2x1 / 3x1; climate 2x1 / 3x1; vacuum 2x1 / 3x1;
  light, switch, automation, binary_sensor 1x1 / 1x1; sensor default 1x1 max 2x1;
  weather default 2x1, min 1x1, max 2x1.
- (new) Clamp reports: cover requested 2x1 landscape -> used 3x1 (reported);
  cover requested 4x2 landscape -> 3x1; cover requested 4x1 portrait -> 2x1;
  light requested 2x1 -> 1x1; sensor requested 3x1 -> 2x1; weather requested 4x1 ->
  2x1; climate requested 2x2 portrait -> 2x1; climate requested 2x2 landscape ->
  3x1; binary_sensor requested 2x1 -> 1x1. A request equal to the default is not
  reported.
- (new) Clamp step 3 (synthetic type table with tall types; the limit is always
  rows - 1): portrait limit 6, so a 1x7 request gives 1x6 and a 1x4 request stays
  1x4; landscape limit 3, so a 1x4 request gives 1x3 (reported).
- (changed) Landscape has 5 columns: 5 x 1x1 widgets fill one row without
  overflow; a 6th 1x1 starts row 1. Portrait: 2 x 1x1 fill a row; a 3rd starts
  row 1.
- (new) Capacity: portrait 7 rows x 2 = 14 cells, 15th 1x1 triggers the pager
  (page 0 then has 6 rows = 12 cells); landscape 4 x 5 = 20 cells, 21st 1x1
  triggers the pager (page 0 has 3 rows = 15 cells).
- (changed) Worked example (section 4 table): portrait cells and rects, and
  landscape cells and rects, exactly as listed there (10 widgets, no pager).
- (new) Overflow to the pager, portrait: 7 wide widgets (2x1) fill 7 rows; an 8th
  makes 2 passes; with 6 rows page 0 holds 6 wide widgets, page 1 holds the 7th
  and 8th at rows 0 and 1; pager visible, `subPageCount` = 2.

### 8.3 Tab groups

- 8 tabs portrait: per=3, G=3; active tab 4 -> group 1 shows tabs 3,4,5.
  (unchanged)
- Tab count 4 portrait: no overflow slot; tab count 5: slots show 3 tabs +
  overflow. (unchanged)
- Landscape: tab count 5: all 5 slots show tabs, no overflow slot. Tab count 6:
  per=4, G=2; slots 0..3 + overflow in slot 4; active tab 5 -> group 1 shows tabs
  4,5 in slots 0,1 and slots 2,3 are `empty`. (unchanged logic, new rects)
- Max 16 tabs: portrait G=6, landscape G=4. (unchanged)

### 8.4 Hit tests

All are (changed) versus the previous geometry unless marked (new).

- Portrait status: (0,0) -> refresh; (287,55) -> refresh; (288,30) -> none;
  (300,30) -> none (indicator region, not a target); (411,30) -> none;
  (412,0) -> settings; (523,55) -> settings; (524,30) -> none; (412,56) -> none
  (below the status bar). (100,56) -> none (gap); (100,67) -> none;
  (16,68) -> widget cell (0,0) if one is placed. The (412,0)/(523,55) `settings`
  vectors are (new).
- Portrait tabs: (16,892) -> tab slot 0; (133,892) -> tab slot 0 (last column of
  the slot); (134,892) -> none (gap); (146,892) -> slot 1; (16,891) -> none (above
  the slot); (16,948) -> none (below); (523,947) -> slot 3 (a tab or the overflow).
- Portrait widgets: (300,100) with a 1x1 at cell (1,0) -> widget, sub 0; (275,100)
  -> none (gap column x=264..275 is between cells: cell 0 ends at x=263, cell 1
  starts 276; so x=264..275 is none); (16,172) -> none (row gap y=172..183 between
  rows 0 and 1).
- Landscape status: (16,4) -> refresh; (159,59) -> refresh; (160,30) -> none;
  (171,30) -> none (gap); (172,4) -> tab slot 0; (291,59) -> tab slot 0;
  (292,30) -> none; (304,4) -> tab slot 1; (172,60) -> none (below the slots);
  (819,30) -> tab slot 4 (or overflow); (820,30) -> none; (831,30) -> none;
  (832,4) -> settings (new); (943,59) -> settings (new); (944,30) -> none.
- Landscape grid: (16,71) -> none (above the grid, 12 px strip 60..71);
  (16,72) -> widget cell (0,0) if placed.
- Sub-regions (new): cover portrait at (16,68,508,104): (296,88) -> sub 1,
  (359,151) -> sub 1, (360,100) -> sub 0 (12 px gap between buttons, x=360..371),
  (372,88) -> sub 2, (448,88) -> sub 3, (511,151) -> sub 3, (512,100) -> sub 0
  (padding), (296,87) -> sub 0 (above the buttons).
  Cover landscape at (16,72,552,104): sub 1 (340,92,64,64), sub 2 (416,92,64,64),
  sub 3 (492,92,64,64).
  Climate portrait at (16,532,508,104): minus (248,552,64,64), plus
  (448,552,64,64); (380,584) -> sub 0 (value area). Climate landscape at
  (16,304,552,104): minus (292,324,64,64), plus (492,324,64,64).
  Vacuum uses the cover rects.
- Rocker: LONG_PUSH in TAB mode returns the effect `OpenSettings` and leaves
  the focus state in TAB mode; LONG_PUSH in WIDGET goes to TAB; in INNER to
  WIDGET (unchanged).
- Rocker: after the last focusable widget of the last sub-page, NEXT gives TAB
  mode with `subPage` unchanged (e.g. 2 of 3 stays 2). With 3 sub-pages where
  only sub-page 0 has focusable widgets, NEXT on its last widget gives TAB mode
  and sub-page stays 0. PREV on the first widget of sub-page 2 when sub-pages 0
  and 1 have no focusable widgets gives TAB mode, sub-page stays 2. (unchanged)
- Rocker: the first event after wake (no ring yet) is applied: NEXT in TAB mode
  from tab 0 gives tab 1 and `ringVisible = true`. (unchanged)
- (new) Dirty rect inflation: a focus ring move on the cover at (16,68,508,104)
  redraws (10,62,520,116); on the light at cell (1,0) portrait
  (276,68,248,104) redraws (270,62,260,116); two horizontal neighbours' inflated
  rects touch (right edge of cell 0 inflated: 264+6 = 270 = left edge of cell 1
  inflated), they do not overlap.

## 9. OPEN items (taste-dependent, ui-designer proposals)

- OPEN-1: with 5 landscape columns a 2-cell widget is 364 px, too narrow for the
  cover/climate/vacuum row layouts, so those types are 3 cells wide in landscape
  (and 2 in portrait). Alternative: 4 landscape columns of 223 px (exact:
  16+4*223+3*12+16 = 960) make "wide" = 2 cells in both orientations, remove the
  per-orientation size table and let 2-cell toggles exist, at the cost of
  capacity (4 x 4 = 16 cells instead of 5 x 4 = 20). Needs a decision.
- OPEN-2: the cell height is 104 (not 112). 112 would give landscape only 3 rows
  (STYLE.md 4, "Why these numbers"). If 112 is preferred, landscape loses a row.
- OPEN-3: Touch wake: the tap that wakes the device is assumed to be consumed
  (not delivered as a tap, otherwise it could hit a tab or widget that was just
  revealed under the sleep indicator). Needs hardware verification.
- OPEN-4: in landscape the sleep indicator hides the tab labels while the device
  sleeps. Alternative: show it only over slots 2..4 (loses the "tap/press" text);
  not done.
- OPEN-5 (was OPEN-4 and OPEN-5, resolved): the portrait status bar is now a real
  56 px bar, so the refresh hit area no longer reaches into hidden space; the
  battery cluster is the touch path to Settings.
