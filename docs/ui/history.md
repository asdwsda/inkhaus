# History popup (sensor graphs)

Task: docs/tasks/048-history-popup-spec.md. Design source: docs/tasks/046-history-research.md (sections 3, 4 and
"Recommended design", accepted 2026-10-29). Mockups (PNGs in docs/ui/png/):
`history-line-portrait.svg`, `history-line-landscape.svg`, `history-step-portrait.svg`,
`history-step-landscape.svg`, `history-bar-portrait.svg`, `history-states.svg`.
Shared values come from STYLE.md (revision 5): fonts F18/F24/F40, centre-line rule (text by `cy`), 64 x 64 targets,
border 2, radius 8, dynamic text INK. The popup rules (modal, 3 px border, 6 px shadow, `close` icon M, closed
before sleep) are **brightness.md section 2, unchanged**; this file only adds what differs.

## 1. Purpose and entities

A read-only modal panel that draws the history of one entity as a graph. Opens on a `sensor` widget (numeric
state) or a `binary_sensor` widget (on/off state). Reads `friendly_name`, the current `state`, `unit_of_measurement`
and the widget's `decimals`; the history itself comes from `GET /api/history/period/...` (phase 1) or
`recorder/statistics_during_period` (phase 2, bars and `hours > 24`) per 046. No service is called. Widgets that
show an attribute (`attribute:`) or whose `history: false` do nothing on long press. Config keys (046 section 4):
`history: { hours, type: auto|line|bar|step, statistic, period, min, max, source }`; absent block = 24 h, line for
numeric, step for on/off.

**Open:** long press (>= 600 ms) on the widget, or rocker long push on the focused widget. A short tap/push keeps its
old meaning. **Close:** close button, tap outside the panel, rocker PUSH, rocker long push. UP / DOWN do nothing
(no focus ring, nothing to move). The popup has no timeout of its own; it closes before the device sleeps
(brightness.md rule 5). Closing aborts a running fetch (the net task checks a cancel flag).

## 2. Greys (new, chart only)

Text, axes, data lines, bars and "on" fills are INK (0) and PAPER (15) as everywhere. The chart adds exactly two greys,
used only for decoration inside the plot (never for text, never as the only carrier of state):

| Name | Hex | Use |
|------|-----|-----|
| GRID | #D0D0D0 (level 13) | 1 px grid lines, the min/max band of the line chart |
| MID | #808080 (level 8) | x-axis tick marks (2 px), step separator hairline, "unavailable" span marker, the in-progress (incomplete) bar |

Greys need the quality refresh, which the popup uses anyway (section 7).

## 3. Layout

Absolute rects. Panel padding 16. Every text/icon sits inside the panel frame minus 12 px; boxes follow the 1.2 rule.
Plot graphics (grid, band, line, bars, lane) are drawn into the plot rect and are not text boxes.

### 3.1 Portrait (540 x 960)

Panel (16, 212, 508, 512), centred in the content area; shadow (22, 218, 508, 512); popup dirty rect
(16, 212, 514, 518).

| Element | Rect / position | Font / size | Grey | Notes |
|---------|-----------------|-------------|------|-------|
| Panel frame | (16, 212, 508, 512) | border 3, r 8 | INK on PAPER | `w-1-frame` |
| Name | x = 32, cy = 260, max width 396 (to x 428) | F24 | INK | truncated with "…" |
| Close button | (444, 228, 64, 64) | `close` M | INK, outline | |
| Value | x = 32, cy = 328, baseline 342, box (32, 304, 200, 48) | F40 + unit F24 (dx 2, same baseline) | INK | current state; unit written without the degree sign (`C`); `-` while unknown. On/off: the state word (`Closed`, `Open`, `On`, `Off`) F40 without unit |
| Range line 1 | right edge x = 508, cy = 315 | F18 | INK | `Last 24 h` / `Last 7 d` / `Last 90 min` |
| Range line 2 | right edge x = 508, cy = 341 | F18 | INK | line: `min 19.9 max 22.7` (window extremes, widget decimals); bar: `total 47.3 kWh`; step: `4x open, 38 min total` |
| Plot rect | (92, 372, 392, 264) | | | x axis = time, W = 392 columns |
| Y tick labels | right edge x = 84, cy = 372 / 504 / 636 | F18 | INK | 3 ticks: top, middle, bottom |
| Grid | horizontal at the 3 ticks (the bottom one is the axis), vertical at every x tick, 1 px | | GRID | |
| Axes | left (91, 368, 2, 269) and bottom (91, 636, 394, 2) | 2 px | INK | |
| X tick marks | 6 px below the axis, 2 px wide | | MID | |
| X tick labels | centred on the tick, cy = 658 | F18 | INK | section 4 |
| Note | x = 32, cy = 700, left (icon `clock` S at x = 32, text at x = 64 when stale) | F18 | INK | section 6 |
| Legend | right edge x = 508, cy = 700; swatch 24 x 12 at cy | F18 | | line: GRID swatch + `min-max`; step: INK swatch `open` + outlined swatch `closed` (text from the device class: Open/Closed, On/Off, ...); bar: MID swatch with 2 px INK outline + `today so far` |

Vertical budget (check): 16 + 64 (title row, to 292) + 12 + 48 (value row, to 352) + 20 (to plot, includes the top tick label
box) + 264 + 22 (x label centre, box to 670.8) + 18 + 22 (footer box to 710.8) + 13 = 512.

### 3.2 Landscape (960 x 540)

Panel (40, 86, 880, 424), centred in the content area (72..524); shadow (46, 92, 880, 424); dirty rect (40, 86, 886, 430).
Name, value and range share the title row.

| Element | Rect / position | Font / size | Notes |
|---------|-----------------|-------------|-------|
| Panel frame | (40, 86, 880, 424) | border 3, r 8 | |
| Name | x = 56, cy = 134, max width 300 (to x 356) | F24 INK | |
| Value | x = 372, cy = 134, baseline 148 | F40 + unit F24 | as portrait |
| Range lines 1 / 2 | right edge x = 824, cy = 121 / 147 | F18 INK | as portrait |
| Close button | (840, 102, 64, 64) | `close` M | |
| Plot rect | (116, 186, 764, 240) | | W = 764 columns |
| Y tick labels | right edge x = 108, cy = 186 / 306 / 426 | F18 | |
| X tick labels | cy = 448 | F18 | |
| Note | x = 56 (icon) / 88 (text when stale), cy = 482 | F18 | |
| Legend | right edge x = 904, cy = 482 | F18 | as portrait |

Plot rect and axes (left (115, 182, 2, 245), bottom (115, 426, 766, 2)) follow the portrait rules. The right end of every plot is
"now": a 24 px free strip between the plot and the panel inner edge keeps the last x label inside the frame.

### 3.3 Chart types

**Line + band (numeric, default).** Per plot column keep `min, max, last` of the data in that column (046 section 2). Order
bottom to top: grid (GRID), band (GRID, 1 px wide vertical min..max bar per column, so it merges into an area), the 2 px INK
polyline through the `last` values (round joins), a filled INK dot r = 5 on the newest value at the right edge. Empty columns
carry the previous value forward (history is a step function). Columns before the first recorded point stay empty. A span in
which the entity was `unavailable`/`unknown` draws no line and no band; a 4 px high MID bar at (x, plot bottom - 8) marks its width
(shown in the landscape mockup). Y axis: data min/max padded 5 % unless `history.min/max` are set, ticks at min / middle / max,
labels with the widget `decimals`; the axis may be rounded outward to values that keep the labels short (mockup: 19.0 / 21.5 / 24.0).
Data below the label region of fixed `min/max` is clipped to the plot edge.

**Step (on/off, default for binary_sensor).** A lane (full inner width, height 96) replaces the plot: portrait (32, 372, 452, 96), landscape
(56, 186, 824, 96), 2 px INK frame. Time spans where the entity was on (open) are INK filled, off is PAPER; any span narrower than 3 px is
drawn 3 px wide so a short event stays visible. Vertical grid (GRID) behind the fills at the x ticks, tick marks (MID) 1..7 px below the lane,
x labels cy = lane bottom + 22 (portrait 490, landscape 304). Below: a 1 px MID hairline at lane bottom + 40 (portrait 508, landscape
322), then the **last changes list**, 4 rows, F24 INK, row cy = hairline + 20 + 36 * i (portrait 528, 564, 600, 636; landscape 342,
378, 414, 450): time `HH:MM` at the left inner edge, state word at +100 px. Newest first; fewer rows if fewer changes.
The step lane has no y axis and no y labels.

**Bar (numeric with `type: bar`, source statistics, phase 2).** One bar per bucket (`period` hour or day), height = the configured
`statistic` (sum/change/mean/max). Plot rect as the line chart; N buckets share the plot width equally: slot = W / N, bar width =
slot - 20 (mockup: 7 slots of 56, bars 36 wide, 10 px either side), bar bottom on the axis (zero baseline), INK fill. The newest
(incomplete) bucket is MID filled with a 2 px INK outline (shape + fill, `today so far` in the legend). Value label F18 INK centred
**above each bar** (cy = bar top - 14), one label per bar only up to 8 bars; more bars (24 hourly) get no value labels, only the y axis.
X labels under the bars (cy = plot bottom + 22): weekday (`Fri`, last bucket `Today`) for day buckets, `HH` for hours. Y axis from 0 (or the
configured `min`) to max rounded up so the tallest bar plus its label fits; ticks 0 / middle / max; no band, no polyline, no dot.

## 4. Time axis

Window = `hours` ending now (right edge). Tick step = the smallest of {1, 2, 3, 6, 12 h, 1, 2, 7 d} that gives at most
`floor(plot width / 90)` ticks (portrait line 4, landscape 8, step portrait 5, step landscape 9). Ticks sit on local
round times (POSIX TZ of the config); no tick at the left or right edge. Labels: step < 24 h: `HH:00`; step >= 1 d: weekday (`Mon`),
beyond 8 d `d.m.`. The 24 h mockups show 18:00 / 00:00 / 06:00 / 12:00 (portrait) and every 3 h (landscape).

## 5. Interactions

| Input | Action | Redraw |
|-------|--------|--------|
| Long press on a sensor/binary widget; rocker long push on it | open (section 7) | popup rect |
| Tap close button / tap outside / rocker PUSH / long push | close, abort fetch | popup rect (quality) |
| Tap inside the panel elsewhere, rocker UP / DOWN | nothing | none |

## 6. States and the note line

The footer note (F18, INK, note rect = left part of the footer, text only, no greys) tells the data age:

| Situation | Note |
|-----------|------|
| Fetch done | `updated 14:05` |
| Cache shown, fetch running | hourglass S + `updating` |
| Cache older than the fresh window (max(10 min, hours x 1 %)), fetch failed / offline | `clock` S + `cached 11:20, offline` (landscape step mockup) |
| Loading, no cache | hourglass S + `fetching` |
| Too many points | alert S + `stopped at 512 KB` |

Sheet: `history-states.svg` (one cell per state; each cell shows the popup's plot rect with its axis frame and the note; heights are
reduced for the sheet, widths follow the portrait plot). In every state the title row (name, close) stays; the value shows `-` and the
range lines are blank unless data exists.

| State | Plot rect shows (centred) | Note |
|-------|---------------------------|------|
| 1 Loading | `hourglass` M, `Loading` F24, `Fetching 24 h of history` F18 | `fetching` |
| 2 No data | `minus` M, `No data` F24, `No readings in the last 24 h` F18 | `updated 14:05` |
| 3 Too many points | `alert` M, `Too many points` F24, `Set source: statistics` F18 | `stopped at 512 KB` |
| 4 Not available | `warn` M, `Not available` F24, reason F18 | reason or `not fetched` |

Axes frame (left + bottom, 2 px INK) is drawn in all four, without ticks and labels. Message block: icon cy = plot top + 36, F24 cy =
plot top + 80, F18 cy = plot top + 108 (portrait plot top 372, landscape 186; plot height 140 in the sheet, so there add the same offsets to the real plot
top: block is horizontally centred in the plot rect).
Reasons for **Not available** (F18): `Bar needs source: statistics` (type bar / statistics source before the WebSocket task), `No graph for this entity type`
(climate, media_player, text states), `HA unreachable` (offline and no cache, with `alert` instead of `warn`), `Entity unavailable`. Too many points: the transfer hit the
512 KB / 15 s limit; nothing is drawn (no partial graph).
Offline with a cache: the cached graph is shown with the `cached` note; no error message.
Entity `unavailable` right now: the graph is shown (history), the value reads `-`.
Stale data: only the note (above); the status bar clock icon is not touched by the popup.

## 7. Refresh sequence (046 section 3)

Everything is drawn into the canvas; the underlying tab is unchanged (no dimming).
1. **Open with a cache hit (fresh or stale):** one `epd_quality` refresh of the popup rect (16, 212, 514, 518) portrait / (40, 86, 886, 430) landscape. The rect holds
   the tab's greys, so fastest is not allowed (STYLE 10). Stale cache: the network fetch starts afterwards.
2. **Open without a cache:** frame 1 = the panel with title row, axes frame and `Loading`, `epd_fastest` over the popup rect (feedback within ~150 ms; old tab
   text may ghost faintly for the 1..4 s of the wait). Frame 2 (data, or any error state 2..4) = **one `epd_quality` refresh of the whole popup rect**, which clears that
   ghosting. If the data arrive within 300 ms frame 1 is skipped.
3. **Stale cache refreshed:** compare the new columns with the shown ones. Unchanged: update only the note rect (text only, `epd_fastest`). Changed: one `epd_quality`
   refresh of the value row + plot + footer (the panel interior).
4. **Close:** one `epd_quality` refresh of the popup rect (it holds greys again). No full refresh; the popup refreshes count toward the ghost-debt rule (STYLE 10), which may run a
   GC16 at the next quiet moment.
No timed redraws; the graph never refreshes by itself while open. Never a full refresh while the popup is open.

Mockup ids: panel `w-1` / `w-1-frame`, text and icons `w-1-*`, close button `w-1-close-btn`; chart graphics have `g-*` ids (not checked);
`bg-*` is the unchanged tab behind. The states sheet uses `w-1..w-4` (one cell each). `check_mockups`: 0 problems.
