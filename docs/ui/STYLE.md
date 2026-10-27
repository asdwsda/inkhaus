# UI style (shared values)

Owner: ui-designer. All screens and `docs/ui/layout.md` / `docs/ui/widgets.md`
reuse the values below. Coordinates are px, origin top-left of the screen as
the user sees it (after rotation). Rect = (x, y, w, h).

## 1. Grey levels

Level n is hex `n*0x11` (0 = black, 15 = white). Only these five are used.

| Name | Level | Hex | Use |
|------|-------|-----|-----|
| INK | 0 | #000000 | text, borders, "on" fills, icons |
| DIM | 4 | #444444 | secondary text (captions, labels like "Target") |
| OFF | 8 | #888888 | unavailable / stale / loading text, dotted borders, hairlines |
| TRACK | 12 | #CCCCCC | empty part of progress bars only (small areas) |
| PAPER | 15 | #FFFFFF | background, "off" fills, text on INK |

Rules: never use a grey as the only carrier of state (always shape or fill as
well); no large areas of grey 1..14 (uneven refresh, ghosting). Text on INK
fills is PAPER.

## 2. Fonts

M5GFX built-in DejaVu vector sizes (px = nominal height). Datum = the anchor
of the text inside its rect; text is vertically centred in its rect unless the
spec says "top".

| Name | Size | Use |
|------|------|-----|
| F18 | 18 | captions, status bar, tab labels, secondary values |
| F24 | 24 | widget names, button labels, state text |
| F40 | 40 | sensor value, target temperature, pager "2 / 3" |
| F56 | 56 | current temperature, big cover position |

Text fitting rule (all widgets): a name tries its preferred size; if it does
not fit in its rect on one line (or the allowed number of lines) it steps
down F24 to F18; if it still does not fit it is truncated with "…". Numbers
never truncate; if a value does not fit, drop one decimal, then step down one
font size.

SVG mockups use `font-family="DejaVu Sans"` with `dominant-baseline="central"`
(= vertical centre datum).

## 3. Spacing

| Name | px |
|------|----|
| Screen margin | 16 |
| Gap between widgets, between touch targets | 16 (minimum for any two targets) |
| Widget inner padding | 8 |
| Border (widgets, buttons) | 2, corner radius 8 |
| Focus ring | 3 thick, drawn 4..7 px outside the focused rect (inside the 16 px gap) |
| Status bar height | 40 |
| Touch target minimum | 80 x 80 |

Ring and dirty-rect inflation: a focused element's redraw region is its rect
inflated by 8 px. Because gaps are 16, inflated neighbours never overlap.

## 4. Screen areas and grid

Cell height is 144 in both orientations; gap between cells is 16.
Cell pitch = cell size + 16.

| | Portrait 540x960 (default) | Landscape 960x540 |
|---|---|---|
| Status bar | (0, 0, 540, 40) | (0, 0, 960, 40) |
| Tab bar | bottom: (0, 848, 540, 112) | left: (0, 40, 160, 500) |
| Content area | (16, 56, 508, 784) | (176, 56, 768, 464) |
| Columns x rows | 2 x 5 | 4 x 3 |
| Cell size w x h | 246 x 144 | 180 x 144 |
| Column x | 16, 278 | 176, 372, 568, 764 |
| Row y | 56, 216, 376, 536, 696 | 56, 216, 376 |
| Pager row (when needed) | row 4: (16, 696, 508, 144) | row 2: (176, 376, 768, 144) |
| Widgets rows when pager shown | 4 | 2 |

A widget of w x h cells has pixel size
`(w*cellW + (w-1)*16) x (h*144 + (h-1)*16)`:

| cells | portrait px | landscape px |
|-------|-------------|--------------|
| 1 wide | 246 | 180 |
| 2 wide | 508 | 376 |
| 3 wide | n/a (clamped to 2) | 572 |
| 4 wide | n/a (clamped to 2) | 768 |
| 1 high | 144 | 144 |
| 2 high | 304 | 304 |
| 3 high | 464 | n/a (landscape has 3 rows, 2 usable with the pager) |
| 4 high | 624 | n/a |

The content area leaves 8 px clear above the tab bar (portrait: grid ends
y=840, tab bar starts y=848) and 20 px below the last landscape row.

## 5. Status bar

Rect (0, 0, W, 40), W = 540 or 960. White, with a 2 px INK rule at its bottom
edge (0, 38, W, 2). Never touchable. All text F18, INK.

| Element | Portrait rect | Landscape rect | Notes |
|---------|---------------|----------------|-------|
| Connection icon (24x24) | (16, 8, 24, 24) | (16, 8, 24, 24) | see below |
| "Updated" label | (48, 0, 300, 40), left | (48, 0, 300, 40), left | `updated 14:05` |
| Battery icon (40x20) | (428, 10, 40, 20) | (848, 10, 40, 20) | body 36x20 + nub 4x10 |
| Battery percent | (476, 0, 48, 40), right | (896, 0, 48, 40), right | `78%` |

Connection icon states (shape, not grey): wifi arcs (online and HA reachable);
single dot (connecting); arcs with a slash (Wi-Fi down or HA unreachable). The
status reflects the last session attempt and stays frozen while asleep.

Updated label: time of the last successful state fetch from the RTC.
Same day: `updated 14:05`. Earlier day: `updated 26.10. 14:05`. Never fetched
(cache empty): `not updated yet`. OPEN: date/time format (24 h, d.m.) as a
setting later.

Stale: age > 2 x refresh_interval (or > 3 h when refresh is off), or the last
fetch failed. Then the label is drawn as a filled INK chip with PAPER text and
a clock icon (S, 24x24): chip rect (44, 6, 190, 28), clock icon at (48, 8),
text starts at x=78. Widget values go to OFF grey (section 8).

Battery icon: fill width = round(32 * pct / 100) inside the 32x16 inner area.
At <= 15 % the fill is replaced by an "!" glyph; charging draws a bolt in the
body. The percent text is always shown. (Critical-battery screen: system
screen, not specified here.)

## 6. Tab bar

Buttons are 80 high, 16 px apart, F18 label centred, up to 2 lines (word
wrap, ellipsis on the 2nd line). Inactive: PAPER fill, 2 px INK border.
Active: INK fill, PAPER label. A 2 px INK rule separates the bar from the
content (portrait: top edge y=848; landscape: right edge x=158).

| | Portrait | Landscape |
|---|----------|-----------|
| Position | bottom | left |
| Bar rect | (0, 848, 540, 112) | (0, 40, 160, 500) |
| Slots | 4 | 5 |
| Slot rect i | (16 + i*131, 864, 115, 80) | (16, 56 + i*96, 128, 80) |
| Slot x (portrait) | 16, 147, 278, 409 | |
| Slot y (landscape) | | 56, 152, 248, 344, 440 |
| Label width | 99 (115 - 16) | 112 (128 - 16) |
| Tabs per group when overflowing | 3 | 4 |
| Overflow slot | slot 3 | slot 4 |

Overflow (more tabs than slots, max 16 tabs): tabs are shown in groups. If
tab count <= slots, all tabs show and there is no overflow slot. Otherwise
the last slot is the overflow button: outline, "…" icon (40) with `g/G`
(F18, current group / group count) below. Tapping it shows the next group of
tabs (wraps to group 1). It changes only the tab bar's buttons, not the active
tab. The active tab is always in the group shown when it becomes active (a tab
change by touch, rocker or load switches the visible group to the one that
contains it). Empty slots in a short last group are left blank (no outline).

## 7. Sub-page indicator ("pager")

Shown only when the active tab has more than one sub-page. It occupies the
whole last grid row (see section 4 table) and is part of the content area
(the layout engine removes that row from widget placement; see layout.md).

| Element | Portrait rect | Landscape rect |
|---------|---------------|----------------|
| Pager rect | (16, 696, 508, 144) | (176, 376, 768, 144) |
| Previous button (120x80) | (16, 728, 120, 80) | (176, 408, 120, 80) |
| Next button (120x80) | (404, 728, 120, 80) | (824, 408, 120, 80) |
| Label `2 / 3` (F40) | (152, 728, 236, 80), centre | (312, 408, 496, 80), centre |

Buttons use the chevron icons (40). At the first sub-page the previous button is
unavailable-styled (dotted OFF border, no action); likewise next on the last
(no wrap by touch; the rocker wraps, see layout.md).

## 8. Icons

Line/solid glyphs on a 24x24 design grid, drawn as vector paths, stroke 2
(scales with size). Sizes: **S = 24** (status bar, badges, inline),
**M = 40** (buttons, widget icons), **L = 64** (weather condition).

| Group | Icons |
|-------|-------|
| Cover | up (solid triangle), stop (solid square), down |
| Devices | bulb, plug (switch), auto (circular arrows, automation), vacuum (disc with bumper), thermometer, gauge (generic sensor), bolt |
| Climate | flame (heat), snow (cool), auto, power (off), fan, drop (dry / humidity) |
| Vacuum | play (start), pause, dock (house with down arrow) |
| Controls | minus, plus, chevron_left, chevron_right, dots (overflow) |
| System | wifi, wifi_dot, wifi_off, battery, clock, hourglass, warn (circle with slash = unavailable), alert (circle with "!" = failed) |
| Weather (L) | sunny, clear_night, partlycloudy, cloudy, rainy, pouring, snowy, fog, windy, lightning, unknown |

Weather state to icon: `sunny`, `clear-night`, `partlycloudy`, `cloudy`,
`rainy`, `pouring`, `snowy`/`snowy-rainy`, `fog`, `windy`/`windy-variant`,
`lightning`/`lightning-rainy`, anything else `unknown`.

## 9. Visual states

Applies to every widget; widget-specific details are in widgets.md.

| State | Look (shape first) | Touch |
|-------|--------------------|-------|
| Off / idle | PAPER fill, 2 px solid INK border, INK content | active |
| On / active | **INK fill**, PAPER content (toggles); for button rows: the button of the current mode is INK-filled | active |
| Pending (command queued or sent, not yet confirmed) | the **requested** result is drawn (e.g. toggle flips to its new fill), border becomes **3 px dashed** (8 on / 6 off), an hourglass (S) replaces the widget badge; for button rows the tapped button gets the dashed border and an hourglass (M) instead of its glyph | active (a further tap on the same control while pending is ignored; on other controls it is queued) |
| Failed (send failed or state not confirmed after the next fetch) | pending is dropped, the real state is drawn, alert (S) badge stays until the next successful fetch | active |
| Unavailable (`unavailable`, `unknown`, missing entity) | PAPER fill, **2 px dotted** (2 on / 4 off) OFF border, content in OFF grey, warn (S) badge, value shows `n/a` or `-`, buttons dotted | none (taps ignored, no redraw) |
| Loading (no cached value yet) | normal frame, values `-`, in OFF grey, no badge | controls active |
| Stale (global: status-bar chip is shown) | value text in OFF grey instead of INK; frame and state fills unchanged | active |
| Focused (rocker) | 3 px INK ring 4 px outside the rect; inside a multi-button widget the active button also gets a 4 px solid border | n/a |
| Read-only widgets (sensor, binary_sensor, weather) | no pending state, never focusable | no feedback |

Every widget has a **badge slot** 24x24 at widget-relative `(W-32, 8)`
(top-right). It shows hourglass (pending), alert (failed), warn (unavailable)
or nothing; it is a distinct dirty region.

Button spec (all button rows): min 80 high, radius 8, border 2, glyph 40
centred. Width formula for n buttons in a row inside a widget of width W:
`bw = floor((W - 16 - 16*(n-1)) / n)`; the row starts at
`x = 8 + floor(rem/2)` with `rem = (W - 16 - 16*(n-1)) - n*bw`; button i is at
`x + i*(bw + 16)`.

## 10. Refresh rules

The firmware owns the mechanics; the UI requires the following.

**Partial refresh (fast mode, only the listed rect):**
- Tap feedback: pending state of the touched control: the control rect (toggle:
  the whole widget; button row: that button; climate +/-: the target value
  rect plus the badge).
- A state value changed by a fetch: the smallest element rect that changed
  (value text, button fill, badge), merged into the widget rect if the changed
  rects cover more than 60 % of the widget area.
- Focus ring moves: old and new element rects, each inflated by 8 px.
- Status bar: the changed element only (label, battery, connection).
- Tab bar: highlight change when tabs are selected with the rocker; overflow
  group switch; the whole tab bar rect.
- Regions are rounded out to 4 px by the driver; the 16 px gaps absorb that.

**Full-quality refresh (whole screen, clears ghosting, flashes):**
- Changing tab, sub-page, or orientation (the content is new).
- Settings screen enter/exit.
- Scheduled (RTC) refresh that changed at least one widget (as in
  ARCHITECTURE.md).
- A fetch that changes more than 6 widgets at once.
- Ghost debt: after 12 partial updates since the last full refresh, or when
  the summed partial area exceeds 150 % of the screen, the next quiet moment
  (no input for 2 s) triggers one; if none occurred, one runs just before
  sleeping. OPEN: tune 12 / 150 % on the device.
- Never refresh when nothing changed. Never redraw on wake before first input
  or data (the screen is already showing the last frame).

**Rocker tab switching** debounce: the tab bar highlight moves immediately
(partial); the content full refresh runs after 400 ms without further
presses.
