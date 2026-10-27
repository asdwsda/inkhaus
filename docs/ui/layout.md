# Layout engine spec

Exact spec for task 006 (`lib/ui`). Values come from `docs/ui/STYLE.md`;
widget internals and sub-region rectangles are in `docs/ui/widgets.md`.
Rect = (x, y, w, h). A point is inside a rect when `x <= px < x+w` and
`y <= py < y+h`.

## 1. Inputs and outputs

Input: orientation (`portrait` | `landscape`), tab list (title + widgets with
type and optional `size`), active tab index, active sub-page index, and the
visible tab group (derived, see 5).

Output `Layout`:
- `statusBar`, `tabBar` rects;
- `tabSlots[]`: slot rects, each with `{kind: tab|overflow|empty, tabIndex}`;
- `pager`: rect, prev/next button rects, `visible` flag, `subPage`,
  `subPageCount`;
- `widgets[]` for the active sub-page: `{index (in tab), type, rect,
  cellX, cellY, cellW, cellH, clamped}`;
- `clampReports[]`: widgets whose requested size was changed.

## 2. Grid geometry

| | Portrait | Landscape |
|---|----------|-----------|
| Screen | 540 x 960 | 960 x 540 |
| Status bar | (0,0,540,40) | (0,0,960,40) |
| Tab bar | (0,848,540,112) | (0,40,160,500) |
| Content area | (16,56,508,784) | (176,56,768,464) |
| Columns / rows | 2 / 5 | 4 / 3 |
| Cell | 246 x 144 | 180 x 144 |
| Gap | 16 | 16 |
| Column x | 16, 278 | 176, 372, 568, 764 |
| Row y | 56, 216, 376, 536, 696 | 56, 216, 376 |

Cell (c, r) origin: `x = colX[c]`, `y = rowY[r]`. A widget of `w x h` cells at
(c, r) has rect `(colX[c], rowY[r], w*cellW + (w-1)*16, h*144 + (h-1)*16)`.
All rects end inside the content area: portrait max right edge 524, bottom 840;
landscape right 944, bottom 520.

## 3. Widget sizes

Size in cells (w x h). "Default" is used when config has no `size`.

| Widget type | Default | Min | Max | Notes |
|-------------|---------|-----|-----|-------|
| cover | 2x1 | 2x1 | 4x2 | 2x2 adds position display (widgets.md) |
| cover group (`group:` entry of covers) | 2x1 | 2x1 | 4x2 | same layout as cover |
| light | 1x1 | 1x1 | 2x1 | |
| switch | 1x1 | 1x1 | 2x1 | |
| automation | 1x1 | 1x1 | 2x1 | |
| climate | 2x2 | 2x2 | 4x2 | |
| sensor | 1x1 | 1x1 | 2x1 | |
| binary_sensor | 1x1 | 1x1 | 2x1 | |
| weather | 2x1 | 1x1 | 4x1 | |
| vacuum | 2x1 | 2x1 | 4x2 | |

Groups: a `group:` entry has the widget type of its members' domain (all
members share one domain). Only `cover` groups are required in v1; light and
switch groups use the light/switch type.

Clamp rule (applied per widget, in this order):
1. `w = clamp(w, min.w, max.w)`; `h = clamp(h, min.h, max.h)` for the type.
2. `w = min(w, columns)`.
3. `h = min(h, rows - 1)` (portrait: 4, landscape: 2), so that every widget
   also fits when the pager row is reserved.

If the result differs from the requested size (config size or default), report
it in `clampReports` (widget index, requested, used). Type minimums never
exceed the grid (largest min is 2x2; portrait has 2 columns and 4 usable rows,
landscape 4 columns and 2 usable rows).

## 4. Flow algorithm and sub-pages

```
place(widgets, rows):                       # rows = 5/3 (no pager) or 4/2 (pager)
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

Worked example (config.example.yaml "Living room" tab: light 1x1, group of
covers 2x1, climate 2x2, sensor 1x1, cover 2x1):

| Portrait (5 rows) | cell (c, r) |
|-------------------|-------------|
| light | (0,0) |
| group | (0,1) - col 1 of row 0 is too narrow for 2 wide |
| climate | (0,2) |
| sensor | (1,0) - first fit fills the gap in row 0 |
| cover | (0,4) |

Everything fits in 5 rows: no pager. Landscape (4 columns x 3 rows):
light (0,0); group (1,0); climate (0,1); sensor (3,0); cover (2,1).
Rects: see `docs/ui/tab-portrait.svg` / `tab-landscape.svg`.

## 5. Tab bar and status bar

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

## 6. Hit-test regions

`hitTest(point) -> {kind, index, sub}` checked in this order; rects from the
Layout; gaps and margins and the status bar return `none`.

| Kind | Region | index | sub |
|------|--------|-------|-----|
| `tab` | a `tab` slot rect | tab index | 0 |
| `tabOverflow` | the overflow slot rect | 0 | 0 |
| `pagerPrev` / `pagerNext` | pager button rects (only if the pager is visible) | 0 | 0 |
| `widget` | sub-region rect (see widgets.md) of a widget on the active sub-page | widget index in tab | sub-region id (>= 1) |
| `widget` | any other point inside the widget rect | widget index | 0 |
| `none` | everything else | | |

Sub-region id 0 means "widget body". Widgets with a single action (toggles)
have no sub-regions: the whole rect is id 0 and triggers the action. For
multi-button widgets, id 0 triggers nothing (the app ignores it). Sub-region
rects never overlap and lie 16 px apart (button rows) so rect edges are the
hit boundary; no extra slop. The layout engine therefore also exports
`subRegions(type, size, widgetRect) -> [{id, rect}]` using the formulas in
widgets.md.

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

**TAB mode** (default). The focus is the active tab; its button shows the
focus ring.
- PREV / NEXT: previous / next tab, wrapping (last -> first). The tab bar
  highlight updates at once (partial); the content switches after 400 ms of no
  presses (full refresh). The visible tab group follows (section 5).
- PUSH: enter WIDGET mode on the first focusable widget of the active sub-page.
  If there is none, no effect.
- LONG_PUSH: force a state refresh now. OPEN: the user may prefer another
  action.

**Focusable widgets**: cover, cover group, light, switch, automation, climate,
vacuum, and only if they are not unavailable. Sensor, binary_sensor and
weather are read-only and skipped.

**WIDGET mode**. Focus stops are, in order: stop 0 = the tab bar (leaves to TAB
mode), then the focusable widgets of the active sub-page sorted by (row, column)
of their top-left cell (stops 1..n). The ring is drawn around the focused widget
rect.
- NEXT on stop k: stop k+1. After the last widget: if another sub-page exists,
  go to sub-page+1 (full refresh), stop 1; if this was the last sub-page, wrap
  to stop 0 (TAB mode, ring on the active tab) .
- PREV on stop k: stop k-1. Before the first widget: if sub-page > 0, go to
  the previous sub-page, last stop (full refresh); on sub-page 0 go to stop 0
  (TAB mode).
- Sub-pages with no focusable widget are skipped in either direction.
- PUSH on a single-action widget (light, switch, automation): run its action.
  PUSH on a multi-button widget (cover, group, climate, vacuum): enter INNER
  mode.
- LONG_PUSH: back to TAB mode.

**INNER mode** (multi-button widgets). The widget ring stays; the active button
gets a 4 px solid border. Order of stops (from widgets.md): cover/group
up, stop, down (entry = stop); climate minus, plus (entry = minus; mode only
if enabled); vacuum start, pause, dock (entry = start).
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

### 7.3 Redraws caused by focus

Ring move: redraw old focus rect and new focus rect, each inflated by 8 px
(partial). Tab/sub-page change: full refresh. Mode change TAB <-> WIDGET:
ring moves only.

## 8. Engine test vectors (for the 006 tests)

- Portrait slot rects: (16,864,115,80), (147,864,115,80), (278,864,115,80),
  (409,864,115,80).
- Landscape slot rects: (16,56,128,80), (16,152,128,80), (16,248,128,80),
  (16,344,128,80), (16,440,128,80).
- 1x1 portrait cell (1,0) = (278,56,246,144); 2x2 at (0,2) = (16,376,508,304).
- 2x1 landscape at (2,1) = (568,216,376,144); 4x2 landscape = (176,y,768,304).
- Pager rects: STYLE.md section 7.
- 8 tabs portrait: per=3, G=3; active tab 4 -> group 1 shows tabs 3,4,5.
- Tab count 4 portrait: no overflow slot; tab count 5: slots show 3 tabs + overflow.
