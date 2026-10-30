# History graph widget (`display: graph`)

Task: docs/tasks/071-history-widget.md. Based on history.md (popup: chart types, greys, time axis, data rules), widgets.md
(conventions, ids, text boxes) and layout.md (sizes, clamp). Shared values: STYLE.md revision 5 (F18/F24/F40, centre-line rule,
text INK, border 2, radius 8, padding 12, badge slot). Mockups (PNGs in docs/ui/png/):
`history-widget-portrait.svg` (2x2 line graph in the tab), `history-widget-landscape.svg` (same, landscape),
`history-widget-states.svg` (step, bar, loading, no data, error, cached, missing entity, compact 2x1). `check_mockups`: 0 problems.

## 1. Purpose and entities

A read-only widget that draws the history graph of one `sensor` (numeric) or `binary_sensor` (on/off) entity straight on the tab.
Same renderer, graph types, data sources and config options as the popup: `history: { hours, type: auto|line|bar|step, statistic,
period, min, max, source }` (history.md section 1); absent block = 24 h, line for numeric, step for on/off. Selected by
`display: graph`. Reads `friendly_name`, current `state`, `unit_of_measurement`, `decimals`; history from the same endpoints as the
popup. No service is called. Name = config `name`, else `friendly_name`. Value text as the sensor widget (decimals, unit; on/off:
state word from the device class).

## 2. Sizes (layout.md section 3 entry)

| | Portrait | Landscape |
|---|----------|-----------|
| Default | 2x2 = 508 x 220 | 2x2 = 364 x 220 |
| Min | 2x1 | 2x1 |
| Max | 2x(rows-1 when a pager is shown, else 4) | (columns)x4, e.g. 5x4 = 928 x 452 |

Width below 2 and height below 1 are clamped up (a 1-wide graph is unreadable) and reported in `clampReports`. A graph never
shrinks other widgets; it flows like any widget (it needs a free w x h block). A 2x2 graph in portrait occupies a full row pair
(portrait: 508 x 220 at the two rows; two graphs stack). Landscape 3x2 (552 x 220) or wider is recommended for 24 h data when
space is free; the same layout applies, only the plot width grows.

## 3. Layout, 2x1 or taller: "full" layout (H >= 220)

Widget-relative rects, `W` x `H` (H = 220 for 2x2; each extra row adds 116). Content box (12, 12, W-24, H-24).

| Element | Rect / position | Font | Grey | Notes |
|---------|-----------------|------|------|-------|
| Frame | (0, 0, W, H) | border 2, r 8 | INK on PAPER | `w-N-frame`; dotted OFF when the entity is missing (section 7) |
| Name | x = 12, cy = 36 (baseline 44), box height 28.8 | F24 | INK | left, truncated with "…" to the width left of the value (minus 12 px) |
| Value | right edge x = W-12, cy = 36 (baseline 50), box (.., 12, .., 48) | F40 + unit F24 (dx 2, same baseline) | INK | `-` while unknown; on/off: state word F40, no unit. Portrait max width 330, landscape 200 |
| Plot rect (line, bar) | (60, 78, W-84, H-134) = portrait 2x2 (60, 78, 424, 86), landscape 2x2 (60, 78, 280, 86) | | | x axis = time, one column per px |
| Axes | left (59, 74, 2, ph+5), bottom (59, 78+ph, pw+2, 2) | 2 px | INK | ph = plot height |
| Y labels | right edge x = 52, cy = 78 (max) and 78+ph (min); middle label at 78+ph/2 only when ph >= 120 | F18 | INK | widget `decimals`; bars: `0` and the axis max |
| Grid | horizontal at top and middle, vertical at every x tick, 1 px | | GRID | |
| X ticks | 2 px wide, 6 px below the axis | | MID | |
| X labels | centred on the tick, cy = H-32 (portrait/landscape 2x2: 188) | F18 | INK | history.md section 4; tick count = floor(pw / 90): portrait 2x2 four (18:00 00:00 06:00 12:00), landscape 2x2 two (00:00, 12:00) |
| Cached marker | `clock` S at (12, H-44, 24, 24), cy = H-32 | | INK | only in the stale state (section 6); icon only |
| Badge slot | (W-36, 57, 24, 24) | | | only the `warn` badge of the missing-entity state |

Vertical budget (2x2, H = 220): header boxes 12..60; plot label box top 67.2 (gap 7.2); plot 78..164; x label box 177.2..198.8
(inside 208); cached marker box 176..200. Horizontal: the plot spans 60..W-24, value dot (r 5) ends at W-19, inside W-12.

### Chart types (history.md section 3.3, adapted)

- **Line** (default numeric): GRID band + 2 px INK polyline of the per-column `last` + INK dot r 5 on the newest value at the
  plot right edge. Y axis from data min/max padded 5 % (or `history.min/max`).
- **Step** (default on/off): a lane replaces the plot, no y axis and no y labels: lane (12, 76, W-24, 80), 2 px INK frame; on spans
  INK filled (min width 3 px), vertical GRID lines at ticks, tick marks MID 1..8 px below the lane, x labels at cy = 76 + 80 + 22 = 178
  (lane bottom + 22). No changes list (it does not fit); the header value shows the current state word. Taller widgets (H >= 336)
  grow the lane to H-140.
- **Bar** (numeric, `type: bar`, statistics): plot rect as line; N buckets share pw; slot = floor(pw / N), bar width = slot - 20, INK
  fill, the newest (incomplete) bucket MID with a 2 px INK outline. Value labels F18 above each bar (cy = bar top - 14) only when
  N <= 8 **and** slot >= 48 (portrait 2x2 with 7 days: slot 60, yes; landscape 2x2: slot 40, no labels, y axis only). Y axis from 0
  (or `min`) to a maximum rounded up so the tallest label box top stays >= 66. X labels: weekday (`Today` last) or `HH`; weekday
  labels are dropped when slot < 40 (every second label is kept).

### Compact layout, H = 104 (2x1)

| Element | Rect / position | Font | Notes |
|---------|-----------------|------|-------|
| Name | x = 12, cy = 36 (baseline 44) | F24 INK | truncated to the left block |
| Value + unit | x = 12, cy = 69 (baseline 77) | F24 INK (unit F24, dx 2) | on/off: state word |
| Left block | portrait (12, 21.6, 200, 61.8); landscape (12, 21.6, 140, 61.8) | | stack A (STYLE 4b) |
| Spark rect | portrait (228, 20, 263, 62); landscape (164, 20, 183, 62) | | |
| Spark axis | bottom 2 px INK at y = 82 | | no labels, no ticks, no grid |
| Content | line: GRID band + 2 px line + dot r 5 (dot centre at W-17); bar: bars without labels; step: lane height 40 centred in the spark rect | | y range = data min/max padded 5 % |

Compact states: icon S + F18 text (`Loading`, `No data`, `No graph`) centred in the spark rect (cy = 52); no axis.
Fill: left block 61.8 / 80 = 77 %, spark 62 / 80 = 78 %.

## 4. Interactions

| Input | Action | Redraw |
|-------|--------|--------|
| Tap anywhere on the widget | nothing | none |
| Long press | nothing (OPEN-1) | none |
| Rocker | the widget is **never focusable** (read-only, like sensor/weather); focus order skips it; UP/DOWN/PUSH never reach it | none |

No tap feedback of any kind: no pending state, no ring, no flicker.

## 5. Data and redraw

- Fetched in every session that fetches states (wake and scheduled refresh), for the **visible tab only** and after the state fetch
  (task 071). Last data is cached; the first frame after a wake is the image that is already on the screen (nothing is redrawn
  before the fetch ends).
- After the fetch: compare the new columns with the shown ones. Unchanged data: draw nothing. Changed data: the plot holds greys,
  so one `epd_quality` partial refresh of the **widget rect** (value and plot change together). If the session is a scheduled
  refresh that changed at least one widget, the GC16 full refresh of STYLE 10 draws it and no separate partial runs.
- Only the value changed (plot columns identical): value rect (right edge W-12, y 12..60) with `epd_fastest` (pure black text).
- Fetch failed with a cached graph: draw/clear only the cached marker (12, H-44, 24, 24) with `epd_fastest` (pure black/white
  region, below the tick marks).
- Counts toward ghost debt (STYLE 10) like any partial refresh. The graph never redraws on its own while the device is awake.

## 6. States

| State | Look |
|-------|------|
| Normal | graph as section 3 |
| Loading (no cache for this widget yet) | header with value `-`; axes frame without ticks and labels; centred in the plot rect: `hourglass` S + `Loading` (F24, cy 108), `Fetching history` (F18, cy 142). Frame stays solid |
| No data (no readings in the window) | header value as known; `minus` S + `No data`, `No readings in 24 h` |
| Error, no cache (HA unreachable, network error) | value `-`; `alert` S + `HA unreachable`, `No cached graph` |
| Too many points (512 KB / 15 s limit) | `alert` S + `Too many points`, `Set source: statistics`; nothing is drawn |
| Cached, last fetch failed or data older than 2 x refresh interval | the cached graph unchanged + `clock` S marker bottom left (12, H-44); no message. The status bar clock icon is the global stale marker as usual |
| Entity `unavailable` now | graph from history, value `-`, no other change (as in the popup) |

Message block (full layout): centred on the plot centre x = 60 + pw / 2; line 1 = icon S (24) + 8 px + F24 text on cy = 108; line 2
F18 cy = 142; both inside the plot rect height (78..164) and the frame. Compact: section 3. All message text INK.

### Missing entity (not in HA)

Unavailable look (STYLE 9): 2 px dotted OFF frame, name in OFF, value `n/a` INK, `warn` badge at (W-36, 57), axes frame, message
`Not available` (F24, cy 108, no icon: the badge carries it) and `Entity not found` (F18, cy 142). No taps anyway.

## 7. Stress and truncation

Names are truncated, never smaller; a value that does not fit its width drops one decimal, then F40 -> F24 (STYLE 2). Long unit
strings (`kWh`) fit portrait; landscape 2x2 value width is limited to 200 px.

## 8. Mockup ids

Widget `w-N`, frame `w-N-frame`, text and icons `w-N-*` (`-name`, `-value`, `-yt`, `-yb`, `-xlN`, `-blN`, `-stale`, `-badge`, `-icon`,
`-msg`, `-sub`); chart graphics `g-*` (not checked). The sheet uses `w-1..w-10`, one per state.

## OPEN

1. Long press on the graph widget: nothing, or open the popup (history.md)? Recommended default: nothing (matches "tap does
   nothing"; the popup stays on sensor/binary widgets).
