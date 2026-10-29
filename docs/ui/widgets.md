# Widget specs

Revision 7 (revision 5: dynamic text black; revision 6: section 11 "State wording"; revision 7: section 12 "Sensor details line"). Revision 4 cause of the revision 3 defects (sensor value outside its frame, a
crowded thermostat): the vertical budgets were computed from centre lines without
the real text box heights. Every widget below now has a **vertical budget table**
built from text boxes of 1.2 x font size.

All coordinates are **widget-relative** `(x, y, w, h)`, origin at the widget's
top-left. `W` is the widget's pixel width, `H = 104` always (STYLE.md section 4):

| Size | Portrait W | Landscape W |
|------|-----------|-------------|
| 1x1 | 248 | 176 |
| 2x1 | 508 ("wide" in portrait) | 364 |
| 3x1 | n/a | 552 ("wide" in landscape) |

"Wide" = 2 cells in portrait (W = 508), 3 cells in landscape (W = 552). Sizes and
clamping: layout.md section 3.

## 0. Conventions used by every widget

- **Frame** (0, 0, W, 104), radius 8, border 2, fill PAPER (INK when "on").
  **Content box** = frame minus 12 px padding on every side = (12, 12, W-24, 80),
  vertically y = 12..92. Every text box, icon and badge lies inside it. Buttons are
  the only exception: 64 x 64 at y = 20..84 (also inside it), and the checker
  ignores `*-btn` outlines.
- **Fonts**: only F18, F24, F40, F56 (STYLE.md section 2). A name never steps down
  to a smaller font: it is truncated with `…` (toggle names: F24 always).
- **Text box** = height 1.2 x font size, centred on the line's centre line `cy`:
  F18 21.6, F24 28.8, F40 48, F56 67.2. Text boxes of neighbouring lines in a
  stack are >= 4 px apart. At most **2 text lines** per 1-row widget.
- **Baseline for the SVG and for mixed runs.** The glyph ink of a line sits on a
  baseline `by = cy + k`, with k = 6 (F18), 8 (F24), 14 (F40), 19 (F56) (this is
  0.346 x size, the DejaVu em-box centre). The SVGs draw widget text with
  `dominant-baseline="alphabetic"` at that `by`, so they do not depend on a viewer's
  `central` support. Firmware may use `middle_left` at `cy` for single-size lines.
- **Value + unit runs** (`21.5°C`, `12°C`): the unit is the same size as the value or
  one step smaller, and **shares the baseline**: both drawn with the M5GFX datum
  `baseline_left` at the same `by` (unit x = value x + value width + 2). No "+6 px"
  offsets. In the SVG it is one `<text dominant-baseline="alphabetic" y="by">` with
  a `<tspan font-size=... dx="2">` for the unit.
- **Names / strings** come from config `name`, else HA `friendly_name`. Truncation
  to a width: drop characters from the end and append `…` (U+2026, one glyph, 1 em
  wide) until it fits.
- Text colour (revision 5): **every text that can change at runtime is INK on
  PAPER, PAPER on INK fills** (state lines, values, units, temperatures, humidity,
  weather condition, vacuum status, battery text, `n/a`, `-`), in every state
  including stale, loading and unavailable. DIM is only for never-changing
  captions (sensor/weather 1x1 names); OFF only for the static title and icons of
  an unavailable widget, and dotted frames.
- "Stale" is global (status-bar chip). A stale widget looks unchanged.
- **Badge slot** (24 x 24): hourglass (pending), alert (failed), warn
  (unavailable), else empty. Position per widget below. **Cover and vacuum show
  pending only in the tapped button**, never as a badge.
- Redraw: partial, only the listed element rects (STYLE.md section 10).
- Button row (three buttons): x = W-228, W-152, W-76; y = 20; 64 x 64; glyph M at
  (x+12, 32). Pending: tapped button 3 px dashed + hourglass M instead of glyph.
- **Fill check** (STYLE.md 4b): each widget gives horizontal span (reserved boxes
  over the inner width W-24) and vertical span (top of the first text box or icon
  to the bottom of the last, over 80). Target: >= 75 % horizontal, >= 60 % vertical.
- **Element ids in the mockups**: `<g id="w-N">`, border `w-N-frame`; text and icons
  `w-N-name`, `w-N-state`, `w-N-icon`, ...; buttons `w-N-<which>-btn` with the glyph
  `w-N-<which>-icon`. Checked by `tools/check_mockups.py`.

### Standard stacks (used below)

Stack A, two F24 lines (cover, vacuum, climate left, weather 2x1):

| Line | Font | cy | Baseline | Box top | Box bottom | Gap below |
|------|------|----|----------|---------|------------|-----------|
| 1 | F24 | 36 | 44 | 21.6 | 50.4 | 4.2 |
| 2 | F24 | 69 | 77 | 54.6 | 83.4 | |

Total 21.6..83.4 = **61.8 px** of the 80 (margin 9.6 above, 8.6 below).

Stack B, value F40 over caption F18 (sensor 1x1, weather 1x1):

| Line | Font | cy | Baseline | Box top | Box bottom | Gap below |
|------|------|----|----------|---------|------------|-----------|
| 1 | F40 | 39 | 53 | 15.0 | 63.0 | 4.2 |
| 2 | F18 | 78 | 84 | 67.2 | 88.8 | |

Total 15.0..88.8 = **73.8 px** of 80 (margin 3 above, 3.2 below).

## 1. Cover (single shutter), wide

HA: entity domain `cover`; state (`open`, `closed`, `opening`, `closing`,
`unavailable`); attribute `current_position` (0..100); `friendly_name`.
Services (004): `cover.open_cover`, `cover.stop_cover`, `cover.close_cover`.
Size: wide x 1 only (portrait 2x1 = 508, landscape 3x1 = 552).

Vertical budget (left block = Stack A; buttons beside it):

| Line | Font | cy | Box top | Box bottom | Gap |
|------|------|----|---------|------------|-----|
| Name | F24 | 36 | 21.6 | 50.4 | 4.2 |
| State | F24 | 69 | 54.6 | 83.4 | |
| Buttons | 64 px | 52 | 20 | 84 | (beside, not stacked) |
| **Sum (text block)** | | | | | **61.8 of 80** |

| Element | Box (x, y, w, h) | cy / baseline | Font / grey | Content |
|---------|------------------|---------------|-------------|---------|
| Name `w-N-name` | (12, 21.6, W-252, 28.8) | 36 / 44 | F24 INK, left | name; right edge W-240 |
| State `w-N-state` | (12, 54.6, W-284, 28.8) | 69 / 77 | F24 INK, left | see below; right edge W-272 |
| Badge | (W-264, 57, 24, 24) | 69 | S icon | **failed / unavailable only** (not pending) |
| Button 1 "up" | (W-228, 20, 64, 64) | 52 | glyph M at (W-216, 32) | sub-region 1 |
| Button 2 "stop" | (W-152, 20, 64, 64) | 52 | glyph M at (W-140, 32) | sub-region 2 |
| Button 3 "down" | (W-76, 20, 64, 64) | 52 | glyph M at (W-64, 32) | sub-region 3 |

Portrait (W = 508): name box (12,21.6,256,28.8), state box (12,54.6,224,28.8), badge
(244,57,24,24), buttons x = 280, 356, 432. Landscape (W = 552): name box
(12,21.6,300,28.8), state box (12,54.6,268,28.8), badge (288,57,24,24), buttons x =
324, 400, 476.

State text: `closed` -> `Closed`; `open` with position p -> `Open p%` (just `Open`
if p is missing or 100); `opening` -> `Opening p%`; `closing` -> `Closing p%` (omit
` p%` if missing). Unavailable -> `n/a`; loading -> `-`. Longest, `Opening 100%`,
is about 160 px at F24, fits both state boxes.

Name widths: the longest realistic name (`Downstairs shutters`, 240 px at F24)
fits the portrait box (256) and the landscape box (300); longer names are
truncated with `…`.

Fill: horizontal 12..W-12 = 100 % (text left, buttons right); vertical buttons 64 /
80 = 80 %, text block 61.8 / 80 = 77 %, both centred about cy = 52.

### Actions

| Sub-region | Action |
|------------|--------|
| 1 up | `open_cover` -> pending |
| 2 stop | `stop_cover` -> pending |
| 3 down | `close_cover` -> pending |
| 0 (body) | nothing |

### States

| State | Look |
|-------|------|
| Normal | outline buttons; `opening` -> button 1 INK-filled (PAPER glyph); `closing` -> button 3 filled; otherwise none filled |
| Pending | tapped button: dashed 3 px border, **one hourglass M in that button instead of its glyph**; no badge, state text unchanged; other buttons unchanged. Confirmed when the next fetch shows the matching state or position change (or after 2 fetches without change -> Failed) |
| Failed | alert badge; buttons normal |
| Unavailable | dotted OFF frame and buttons, OFF glyphs, warn badge, state `n/a` (INK) |
| Loading | state `-` (INK), buttons normal |
| Stale | unchanged (status-bar chip only) |

Redraw: state box; the two affected buttons when the fill moves (old and new);
badge; the tapped button on pending. Rocker INNER focus: tapped button border.

## 2. Cover group, wide

HA: members' states and `current_position` (fetched once per distinct entity,
shared with single-cover widgets); name is required in config (`name`).
Services: the same three, with `entity_id` listing **all member ids** (004).
Layout, vertical budget, sub-regions, sizes and actions: identical to Cover.

Aggregation for the state text:
1. any member `opening` -> `Opening`; any `closing` -> `Closing` (position
   omitted);
2. else all `closed` -> `Closed`;
3. else all available members have the same `current_position` p -> `Open p%`;
4. else `Mixed`.

Button fill: `Opening` -> up filled, `Closing` -> down filled.
Unavailable only if **all** members are unavailable. If some members are
unavailable: state keeps going from available ones and the badge shows the
alert icon (not warn); controls stay active.

## 3. Light, 1x1

HA: domain `light`; state `on`/`off`; attribute `brightness` (0..255,
percent = round(b*100/255)); `friendly_name`. Service: `light.toggle`.
Size 1x1 only.

**Toggle layout (light, switch, automation, binary_sensor).** Line 1 is the name,
F24, over the full content width; line 2 is the icon M with the state text to its
right. Reason: the name needs the full width (`Floor lamp` 127 px and `Night
mode` 140 px at F24 only fit 152 px if they are not squeezed next to an icon), so
the name font is F24 everywhere and never steps down.

Vertical budget:

| Line | Font / size | cy | Baseline | Box top | Box bottom | Gap below |
|------|-------------|----|----------|---------|------------|-----------|
| 1 Name | F24 | 30 | 38 | 15.6 | 44.4 | 4.6 |
| 2 Icon | M 40 | 69 | n/a | 49.0 | 89.0 | |
| 2 State (beside icon) | F24 | 69 | 77 | 54.6 | 83.4 | (inside icon span) |
| **Sum** | | | | 15.6..89.0 | | **73.4 of 80** |

| Element | Box (x, y, w, h) | Font / grey |
|---------|------------------|-------------|
| Name `w-N-name` | (12, 15.6, W-24, 28.8) | F24 INK / PAPER on INK, left; truncated with `…` |
| Icon `w-N-icon` | (12, 49, 40, 40) | bulb (plug, auto, window); INK, or PAPER on INK |
| State `w-N-state` | (58, 54.6, W-98, 28.8) | F24 INK / PAPER on INK, left; right limit W-40 |
| Badge | (W-36, 57, 24, 24) | S; right end of line 2 |

Landscape (W = 176): name box (12,15.6,152,28.8), state box (58,54.6,78,28.8), badge
(140,57,24,24). Portrait (W = 248): name (12,15.6,224,28.8), state (58,54.6,150,28.8),
badge (212,57,24,24).

State text: off -> `Off`; on -> `On`, or `On 80%` when brightness is known (about 95
px at F24; it ends at x = 153 and would run under the badge at 140, so **while a
badge is shown the text is just `On`**). Bulb icon: outline (`bulb_off`) when off,
solid (`bulb`) when on.

Name widths at F24 (DejaVu): `Ceiling` 82 px, `Floor lamp` 127 px, `Night mode`
140 px: all fit the landscape name width 152 without truncation. Longer names
are cut with `…` (landscape: about 13 characters).

Fill: horizontal name 152 / 152 = 100 %; vertical 73.4 / 80 = 92 % (icon 40 / 80 =
50 % on its own line).

Sub-regions: none. The whole widget rect is the toggle (id 0). Action: toggle.

| State | Look |
|-------|------|
| Off | PAPER, solid border |
| On | **INK fill**, PAPER content |
| Pending | flips to the requested fill at once; dashed 3 px border; hourglass badge |
| Failed | real state; alert badge |
| Unavailable | dotted OFF frame, OFF title and icon, state `n/a` (INK), warn badge, taps ignored |
| Loading | state `-`, PAPER fill |
| Stale | unchanged (the status-bar chip carries it) |

Redraw: the whole widget rect on any change (small, one region). Brightness-only
change: state box.

## 4. Switch, 1x1

HA: domain `switch`; state `on`/`off`. Service: `switch.toggle`.
Same layout, budget, states and redraw as Light with the plug icon; no brightness.

## 5. Automation, 1x1

HA: domain `automation`; state `on` (enabled) / `off` (disabled).
Services: `automation.turn_off` when on, `automation.turn_on` when off
(004). Layout and budget as Light with the auto icon. State text is `On` / `Off`
(enabled / disabled). On = INK fill. Pending / failed / unavailable identical to
Light.

## 6. Climate (thermostat), wide, one row

HA: domain `climate`; state (= hvac mode); attributes `current_temperature`,
`temperature` (target), `current_humidity`, `hvac_mode`, `hvac_action`,
`min_temp`, `max_temp`, `target_temp_step`, `friendly_name`.
Service: `climate.set_temperature` (clamped to min/max, rounded to step by
the action builder, 004). Size: wide x 1 only (portrait 2x1, landscape 3x1).

Left block = Stack A (name, then current temperature + humidity + action on one
line); right cluster `[-] target [+]` centred on cy = 52, no caption.
`Lx` = left block right edge = W-288 (portrait 220, landscape 264).

Vertical budget:

| Line | Font / size | cy | Baseline | Box top | Box bottom | Gap below |
|------|-------------|----|----------|---------|------------|-----------|
| 1 Name | F24 | 36 | 44 | 21.6 | 50.4 | 4.2 |
| 2 Temp / humidity / action | F24 text, S icons | 69 | 77 | 54.6 | 83.4 | |
| **Sum (left block)** | | | | | | **61.8 of 80** |
| Target value | F40 | 52 | 66 | 28.0 | 76.0 | single line |
| Buttons | 64 px | 52 | n/a | 20 | 84 | |

(The current temperature is F24, not F40: F40 on line 2 would need
28.8 + 4 + 48 = 80.8 px, which does not fit 80. The target value keeps F40.)

| Element | Box (x, y, w, h) | Font / grey | Content |
|---------|------------------|-------------|---------|
| Name `w-N-name` | (12, 21.6, W-328, 28.8) | F24 INK, left | name; portrait box width 180, landscape 224 |
| Badge | (W-312, 24, 24, 24) | S | pending / failed / unavailable; right of the name box, cy 36 |
| Current temp `w-N-temp` | starts x = 12, baseline 77, box h 28.8 | F24 INK | `21.5°` (degree sign only, no `C`: the unit is the same F24 and the button value uses it too) |
| Humidity icon `w-N-hum-icon` | (92, 57, 24, 24) | S `drop` INK | omitted with its text when `current_humidity` is unknown |
| Humidity text `w-N-hum-text` | starts x = 120, baseline 77 | F24 INK | `45%` |
| Action icon `w-N-act-icon` | (186, 57, 24, 24) | S INK | see below |
| Button "-" | (W-276, 20, 64, 64) | glyph M at (W-264, 32) | sub-region 1 |
| Target value `w-N-target` | centred at cx = W-144, baseline 66 | F40 INK | `22.0°` (decimals: 1 if step < 1 else 0) |
| Button "+" | (W-76, 20, 64, 64) | glyph M at (W-64, 32) | sub-region 2 |

Portrait (W = 508): minus (232,20,64,64), value centre x = 364, plus (432,20,64,64).
Landscape (W = 552): minus (276,20,64,64), value centre x = 408, plus
(476,20,64,64).

**Line 2 horizontal budget (F24 widths, DejaVu):** `21.5°` = 65 px (12..77), 15 px
gap, drop icon 92..116, 4 px, `45%` = 53 px (120..173), 12 px gap, action icon
186..210. Ends at x = 210 < Lx = 220 (portrait), 10 px spare and 22 px to the minus
button (232). Worst realistic: `-12.5°` (74 px) with `99%` (53 px) still ends at
x = 217 (< 220): the line has room for 99 % humidity and one negative decimal temperature. If
the line would still pass Lx (humidity `100%`), the humidity drop icon is dropped
(text stays) so the action icon keeps its place.

**Action icon** (hvac_action): `flame` heating, `snow` cooling, `dry` drying,
`fan` fan, `pause` idle. Unknown or `off`: the mode icon (`flame` heat, `snow` cool,
`auto` heat_cool/auto, `power` off, `fan` fan_only, `dry` dry). The humidity drop
is never an action icon; the humidity and action icons always differ. The `flame`
has two tongues and a hollow in the middle so that it cannot be mistaken for the
solid, symmetric drop. Mode and action are read-only: no hit region, no rocker stop.

**Target between the buttons.** The value is centred between the buttons
(portrait: minus ends x = 296, plus starts 432, 136 px of room). `28.5°` at F40 is
109 px wide, leaving 13.5 px each side; `Off` is narrower. Value vertical box
28..76 is centred on the button centre line 52.

Fill: horizontal 12..W-12 = 100 %; vertical: left block 61.8 / 80 = 77 %, cluster 64 /
80 = 80 %.

Actions:

| Sub-region | Action |
|------------|--------|
| 1 "-" | target := target - step (min_temp bound) |
| 2 "+" | target := target + step (max_temp bound) |
| 0 | nothing |

Taps change the displayed target at once (pending look); the service call is sent
once, 1.5 s after the last tap or immediately when the session ends. Reaching
min/max leaves the value unchanged (no pending).

States:

| State | Look |
|-------|------|
| Normal | as above; `hvac_mode off` shows target `Off` (F40) and the `power` icon as action icon; buttons still active |
| Pending | the tapped button gets the 3 px dashed border, the badge shows the hourglass, the target value is the requested one |
| Failed | real target; alert badge |
| Unavailable | dotted OFF frame, buttons dotted, line 2 shows `n/a` (F24 INK, no icons), target `n/a` (F40 INK), warn badge |
| Loading | temperatures `-`; buttons active (a tap needs a known target, so it is ignored until the first value) |
| Stale | unchanged (status-bar chip only) |

Redraw: line 2 (box (12, 54.6, Lx-12, 28.8)); target value box; badge. A target tap
redraws the tapped button, the target value box and the badge only.

## 7. Sensor, 1x1 or 2x1

HA: domain `sensor`; state (numeric or text); attributes
`unit_of_measurement`, `device_class`; `friendly_name`. Read-only.
Sizes 1x1 (default), 2x1. Not focusable, no hit regions.

### 1x1 (W = 176 or 248): centred 2-line stack (Stack B)

Vertical budget:

| Line | Font | cy | Baseline | Box top | Box bottom | Gap below |
|------|------|----|----------|---------|------------|-----------|
| 1 Value + unit | F40 value, F24 unit | 39 | 53 (shared) | 15.0 | 63.0 | 4.2 |
| 2 Name | F18 | 78 | 84 | 67.2 | 88.8 | |
| **Sum** | | | | 15.0..88.8 | | **73.8 of 80** |

| Element | Box (x, y, w, h) | Font / grey |
|---------|------------------|-------------|
| Value + unit `w-N-value` | centred at cx = W/2, box (12, 15, W-24, 48) | value F40 INK, unit F24 INK, same baseline 53 (`baseline_left` for both) |
| Name `w-N-name` | (12, 67.2, W-24, 21.6), centred | F18 DIM (static caption), one line, truncated with `…` |
| Badge | (W-36, 12, 24, 24) | S; used only with the short `n/a` / `-` value, which does not reach it |

Value and unit are one centred group: group width = value width + 2 + unit width.
`8.4°C`: value 64 px (F40), unit 29 px (`°C` at F24), group 94 px; start x = W/2 - 47
(41 in landscape, 77 in portrait). Unit appended without space for `°C`/`°F`/`%`,
with a space otherwise.

Checks (landscape, content width 152): `8.4°C` 94 px; **`-12.5°C` = 104 + 2 + 29 = 135
px, fits**, with 8.5 px on each side. Check on height: the value box 15..63 and the
name box 67.2..88.8 lie inside 12..92, 4.2 px apart; no part of the value touches
the frame (digits cap height 29 px: ink 24..53). If a value is wider than W-24 the
rule is: drop one decimal, then the value goes to F24 with the unit at F18
(same baseline); never wrap.

Names: one line only at F18, width 152 (landscape) / 224 (portrait), truncated
with `…` (e.g. `Living room temperat…`). No wrapping.

Non-numeric state text: F24 INK centred in the value box (baseline 53 + the F24
offset: cy 39, baseline 47), single line truncated with `…`.

Fill: horizontal reserved 152 / 152 = 100 % (typical group 62 %); vertical 73.8 / 80
= 92 %.

### 2x1 (W = 364 or 508): row

Vertical budget:

| Element | Font | cy | Baseline | Box top | Box bottom |
|---------|------|----|----------|---------|------------|
| Value + unit | F56 value, F40 unit | 52 | 71 (shared) | 18.4 | 85.6 |
| Name, one line | F24 | 52 | 60 | 37.6 | 66.4 |
| Name, two lines (Stack A) | F24 x 2 | 36 / 69 | 44 / 77 | 21.6 / 54.6 | 50.4 / 83.4 |

Sum: value 67.2 of 80; name 28.8 or 61.8 of 80 (the two stacks never coexist).

| Element | Box (x, y, w, h) | Font / grey |
|---------|------------------|-------------|
| Name `w-N-name` | (12, y per above, W-232, h) | F24 INK, left; up to 2 lines, then `…` |
| Value + unit `w-N-value` | right-aligned at x = W-12, box (W-208, 18.4, 196, 67.2) | value F56 INK, unit F40 INK, same baseline 71 |
| Badge | (W-224, 40, 24, 24) | S; only with the short `n/a`, which is 88 px wide |

Widths: `-12.5°C` = F56 145 + 2 + F40 48 = 195 px = the value box, so the name column
is W-232 (landscape 132, portrait 276). Fill: horizontal 100 %, vertical 67.2 / 80 =
84 %.

| State | Look |
|-------|------|
| Normal | INK |
| Unavailable | value `n/a` in INK, warn badge, dotted OFF frame |
| Loading | value `-` INK |
| Stale | unchanged (status-bar chip only) |

Redraw: value group box only. Pending: n/a.

## 8. Binary sensor, 1x1

HA: domain `binary_sensor`; state `on`/`off`; attribute `device_class`;
`friendly_name`. Read-only, 1x1, not focusable.

**Wording by device_class (revision 5):** `window` and `door` show
`Open` (state on) / `Closed` (state off). Every other or missing device_class
shows `On` / `Off` (icon fallback `dot_on` / `dot_off`: filled / outline ring).
Wording and icons of the other classes: section 11.1. The mockup
shows a window sensor that is open.

Same toggle layout, budget and boxes as Light (section 3), with a sensor icon
instead of the bulb: **`window_on`** (window outline, 18 x 18 of the 24 grid,
cross mullions, panes **filled INK**) when on, **`window_off`** (the same outline,
panes empty) when off. State is carried by fill, not by grey.

| Element | Box (x, y, w, h) | Content |
|---------|------------------|---------|
| Name `w-N-name` | (12, 15.6, W-24, 28.8) | F24 INK, truncated with `…` |
| Icon `w-N-icon` | (12, 49, 40, 40) | window_on / window_off |
| State `w-N-state` | (58, 54.6, W-98, 28.8) | F24 INK: `Open`/`Closed` (window, door; on = open) or `On`/`Off` |
| Badge | (W-36, 57, 24, 24) | S (not used: the icon shows unavailable) |

The frame stays PAPER in both states (the icon carries the state; the widget
is read-only so the INK fill is reserved for things you can toggle).
Unavailable: dotted OFF frame, icon replaced by the `warn` M icon in OFF, state
`n/a` (INK). Vertical budget 73.4 of 80 as Light. Redraw: icon + state box.

## 9. Weather, 2x1 (default) or 1x1

HA: domain `weather`; state = condition; attributes `temperature`, `humidity`,
`temperature_unit`; `friendly_name`. Read-only. Not focusable.

### 2x1 (W = 364 or 508)

Left block = Stack A with the humidity moved onto the condition line; the
temperature is alone on the right (one line, no 2-line stack there).

Vertical budget:

| Line | Font / size | cy | Baseline | Box top | Box bottom | Gap below |
|------|-------------|----|----------|---------|------------|-----------|
| 1 Name | F24 | 36 | 44 | 21.6 | 50.4 | 4.2 |
| 2 Condition + humidity | F24, S icon | 69 | 77 | 54.6 | 83.4 | |
| **Sum (left block)** | | | | | | **61.8 of 80** |
| Temperature (right) | F40 value, F24 unit | 52 | 66 (shared) | 28.0 | 76.0 | single line |
| Icon | L 48 | 52 | n/a | 28 | 76 | |

| Element | Box (x, y, w, h) | Font / grey |
|---------|------------------|-------------|
| Condition icon `w-N-icon` | (12, 28, 48, 48) | L, INK |
| Name `w-N-name` | (68, 21.6, W-174, 28.8) | F24 INK, left, truncated with `…` |
| Condition `w-N-cond` | starts x = 68, baseline 77 | F24 INK, left |
| Humidity icon `w-N-hum-icon` | (x_c + 12, 57, 24, 24) | S `drop` INK; x_c = end of the condition text |
| Humidity text `w-N-hum-text` | starts at drop x + 28, baseline 77 | F24 INK: `76%` |
| Temperature `w-N-temp` | right-aligned at x = W-13, baseline 66 | value F40 + unit F24 (`baseline_left`), INK; `12°C` (integer) |
| Badge | (68, 57, 24, 24) | S, only when unavailable (condition line is then empty) |

Widths (landscape W = 364): temperature `12°C` = 51 + 2 + 29 = 82 px, so it starts
at x = 270 (`-12°C` = 96 px, x = 256). Condition + humidity run: `Cloudy` 83 px
(68..151), 12 px gap, drop 163..187, `76%` 53 px 191..244. Gap to the temperature
(270) is 26 px. Worst case `Pouring` (91 px) ends the run at x = 252, 18 px to the
temperature. Name box ends at x = W-106: 258 (landscape) / 402 (portrait). Portrait
has more room; the run is left-aligned in both.

Condition text (short): Sunny, Clear, Cloudy, Partly, Rain, Pouring, Snow, Fog,
Wind, Storm, else the raw state truncated. Humidity (with its drop) is omitted if
unknown.

Fill: horizontal 12..W-12 = 100 %; vertical: left block 61.8 / 80 = 77 %, icon and
temperature 48 / 80 = 60 %.

### 1x1 (W = 176 or 248): Stack B

Vertical budget:

| Line | Font | cy | Baseline | Box top | Box bottom | Gap below |
|------|------|----|----------|---------|------------|-----------|
| 1 Temperature | F40 value, F24 unit | 39 | 53 (shared) | 15.0 | 63.0 | 4.2 |
| 2 Condition | F18 | 78 | 84 | 67.2 | 88.8 | |
| **Sum** | | | | 15.0..88.8 | | **73.8 of 80** |
| Icon | L 48 | 52 | n/a | 28 | 76 | beside the stack |

| Element | Box (x, y, w, h) | Font / grey |
|---------|------------------|-------------|
| Condition icon `w-N-icon` | (12, 28, 48, 48) | L, INK |
| Temperature `w-N-temp` | starts x = 64, baseline 53 | value F40 + unit F24 (`baseline_left`) INK; `12°C` |
| Condition `w-N-cond` | (64, 67.2, W-76, 21.6), baseline 84 | F18 INK, left, truncated with `…` (dynamic) |
| Badge | (64, 66, 24, 24) | S, unavailable only (condition empty) |

Widths: column x = 64..164 = 100 px; `-12°C` = 96 px fits; `Pouring` at F18 = 68 px.
No name and no humidity at 1x1. Fill: horizontal 12..W-12 = 100 %; vertical text
73.8 / 80 = 92 %, icon 48 / 80 = 60 %.

States (both): Unavailable: icon `unknown` OFF, temperature `n/a` (F40 INK), dotted
frame, warn badge, no humidity and no condition. Loading: `-`. Stale: unchanged. Redraw: icon + temperature + humidity + condition boxes individually.

## 10. Vacuum, wide

HA: domain `vacuum`; state (`cleaning`, `docked`, `idle`, `paused`,
`returning`, `error`, `unavailable`); attributes `battery_level` (0..100),
`status` (text, used only for the error text), `friendly_name`.
Services (004): `vacuum.start`, `vacuum.pause`, `vacuum.return_to_base`.
Size: wide x 1 only (portrait 2x1, landscape 3x1).

Same grid and vertical budget as Cover (Stack A: name cy 36, status line cy 69;
text block 61.8 of 80; buttons at the right):

| Element | Box (x, y, w, h) | cy | Font / grey |
|---------|------------------|----|-------------|
| Name | (12, 21.6, W-252, 28.8) | 36 | F24 INK, left |
| Status text | starts x = 12, box h 28.8 | 69 | F24 INK: `Cleaning`, `Docked`, `Idle`, `Paused`, `Returning`, `Error` |
| Battery icon S | 12 px right of the status text | 69 | S battery (fill = pct) |
| Battery text | 4 px right of the icon | 69 | F18 INK: `85%` (box h 21.6) |
| Badge | (W-264, 57, 24, 24) | 69 | S, failed / unavailable only |
| Button 1 "start" | (W-228, 20, 64, 64) | 52 | play M, sub-region 1 |
| Button 2 "pause" | (W-152, 20, 64, 64) | 52 | pause M, sub-region 2 |
| Button 3 "dock" | (W-76, 20, 64, 64) | 52 | dock M, sub-region 3 |

The status line group must end before the badge, x <= W-272: widest `Returning`
(118 px) + 12 + 24 + 4 + `100%` at F18 (52 px) = 210, ends at x = 222 (portrait
limit 236). Fill: as Cover (100 % / 80 % / 77 %).

Actions: 1 -> `vacuum.start`; 2 -> `vacuum.pause`; 3 -> `vacuum.return_to_base`;
0 -> none. All -> pending (tapped button dashed + hourglass M, no badge).

Fill (current mode): `cleaning` -> start filled; `paused` -> pause filled;
`docked` or `returning` -> dock filled; `idle`/`error` -> none. `error` shows
the `status` text in the status position (truncated, no battery group) and the
alert badge.

States: Pending (confirmed by the next fetch with a different state); Unavailable
(dotted, `n/a`, battery `-`, buttons dotted); Loading (`-`); Stale (unchanged). Battery <= 20 %: battery icon with the "!" glyph (rule in section 11.2).

Redraw: status line, affected buttons, badge.

## Summary table

| Widget | Sizes (portrait / landscape) | Sub-regions (id) | Focusable | Read-only |
|--------|------------------------------|------------------|-----------|-----------|
| cover / group | 2x1 / 3x1 | 1 up, 2 stop, 3 down | yes (INNER mode, entry = stop) | no |
| light, switch, automation | 1x1 | whole widget = id 0 | yes (push = action) | no |
| climate | 2x1 / 3x1 | 1 minus, 2 plus (mode is display-only, no sub-region) | yes (INNER, entry = minus) | no |
| sensor | 1x1, 2x1 | none | no | yes |
| binary_sensor | 1x1 | none | no | yes |
| weather | 1x1, 2x1 | none | no | yes |
| vacuum | 2x1 / 3x1 | 1 start, 2 pause, 3 dock | yes (INNER, entry = start) | no |

Sub-region rects for the engine (`subRegions`), widget-relative: cover / group /
vacuum: `(W-228, 20, 64, 64)`, `(W-152, 20, 64, 64)`, `(W-76, 20, 64, 64)`;
climate: minus `(W-276, 20, 64, 64)`, plus `(W-76, 20, 64, 64)`. Unchanged in
revision 4 (so layout.md section 8 is unchanged).

## Fill check summary

| Widget | Horizontal | Vertical | Meets ~25 % empty? |
|--------|-----------|----------|--------------------|
| cover, group, vacuum (wide) | 100 % | 80 % (buttons), 77 % (text) | yes |
| climate (wide) | 100 % | 77 % (left), 80 % (cluster) | yes |
| sensor 1x1 | 100 % reserved (62 % typical ink) | 92 % | yes |
| sensor 2x1 | 100 % | 84 % | yes |
| sensor 1x1 / 2x1 with details (section 12) | 100 % reserved | 1x1 92 %, 2x1 74 % (left column), 84 % (value) | yes |
| weather 2x1 | 100 % | 77 % (left), 60 % (icon, temperature) | yes |
| weather 1x1 | 100 % | 92 % (text), 60 % (icon) | yes |
| light, switch, automation, binary_sensor 1x1 | 100 % | 92 % (text + icon), 50 % (icon alone) | yes |

## Budget totals (revision 4)

| Widget | Text/content stack height | Limit |
|--------|---------------------------|-------|
| cover, group, vacuum | 61.8 | 80 |
| toggles, binary_sensor | 73.4 | 80 |
| climate left block | 61.8 | 80 |
| sensor 1x1 | 73.8 | 80 |
| sensor 2x1 | 67.2 (value) / 61.8 (two-line name) | 80 |
| weather 2x1 left block | 61.8 | 80 |
| weather 1x1 | 73.8 | 80 |

## 11. State wording (revision 6)

All state texts are F24 INK (dynamic text is black). Widths use 0.55 em = 13.2 px per
character at F24; DejaVu's real widest letters (D, U, O, m, w) run about 10 % wider,
so **binary_sensor texts are capped at 7 characters** (about 100 px real).

**State box of binary_sensor (changes section 8):** the widget never shows a badge, so
the state box is widened to `(58, 54.6, W-70, 28.8)` (right limit W-12): landscape
106 px, portrait 178 px. The 7-character cap fits the landscape box with margin.

### 11.1 binary_sensor by device_class

HA state `on` -> On text, `off` -> Off text. Icon variants come in pairs:
`<name>_on` = solid / filled parts, `<name>_off` = same outline, parts empty (state by
shape and fill, like `window_on`/`window_off`). The frame stays PAPER.

| device_class | on text | off text | Icon on / off | New icon |
|---|---|---|---|---|
| door | Open | Closed | `door_on` / `door_off` | door: door leaf in a frame, handle dot; on = leaf swung open (filled wedge) |
| garage_door | Open | Closed | `garage_on` / `garage_off` | garage: house outline with a roller door; on = door raised (filled gap), off = slats drawn |
| window | Open | Closed | `window_on` / `window_off` | existing |
| opening | Open | Closed | `window_on` / `window_off` | existing (generic opening) |
| motion | Motion | Clear | `motion_on` / `motion_off` | motion: walking figure with two arcs; off = outline figure, no arcs |
| occupancy | Present | Empty | `person_on` / `person_off` | person: head circle + shoulders, solid / outline |
| presence | Home | Away | `person_on` / `person_off` | (shared with occupancy) |
| moisture | Wet | Dry | `puddle_on` / `puddle_off` | puddle: drop sitting on a wavy line (NOT the plain `drop`, which is humidity only), solid / outline |
| smoke | Smoke | Clear | `smoke_on` / `smoke_off` | smoke: cloud with wavy rising lines, solid / outline |
| gas | Gas | Clear | `gas_on` / `gas_off` | gas: cylinder with valve and cloud puff, solid / outline |
| problem | Problem | OK | `problem_on` / `problem_off` | problem: triangle with "!", solid / outline (differs from `alert`, a circle, which means failed) |
| battery | Low | OK | `battery_low` / `battery` | battery_low: existing `battery` shape, M size, nearly empty with the "!" glyph; off = existing `battery` full |
| connectivity | Online | Offline | `wifi` / `wifi_off` | existing |
| lock (on = unlocked) | Open | Locked | `lock_open` / `lock_closed` | lock_open: padlock with raised shackle (outline body); lock_closed: padlock, solid body, closed shackle |
| plug | Plugged | Out | `plug` / `plug_off` | plug_off: the `plug` shape as outline (existing `plug` stays solid) |
| power | Power | None | `bolt` / `bolt_off` | bolt_off: the `bolt` as outline |

Notes: `lock` "on" is the alarming state (unlocked), so it reads `Open` with the open
padlock; `Unlocked` (about 111 px real) would not fit the landscape box. Any other or
missing device_class: `On` / `Off`, `dot_on` / `dot_off`. State `unavailable` or
`unknown`: `n/a`, `warn` icon OFF, dotted frame (as section 8).

### 11.2 Vacuum

The status line has the wide-widget room (group must end at x <= W-272; widest
`Returning` is 9 x 13.2 = 119 px), so only these words are used.

| HA state | Status text | Start / pause / dock fill (section 10) | Notes |
|---|---|---|---|
| cleaning | Cleaning | start filled | battery group shown |
| docked | Docked | dock filled | battery group shown |
| returning | Returning | dock filled | battery group shown |
| paused | Paused | pause filled | battery group shown |
| idle | Idle | none | battery group shown |
| error | the `status` attribute (truncated with `…` to the line limit), else `Error` | none | alert badge, no battery group |
| unavailable / other | `n/a` (unknown: `-`, normal look; other known strings: humanised, 11.4) | none | unavailable: dotted frame, battery `-` |

Low battery: the battery icon S gets the "!" glyph (as in the status bar) when
`battery_level` <= 20 % **and** the state is not `docked` (a docked vacuum at 20 % is
charging, which is normal). The percent text stays `20%` etc. in INK, no colour change.
Missing `battery_level`: the whole battery group is omitted.

### 11.3 Cover

Position p = `current_position` (0..100). Text widths: `Opening 100%` 12 chars =
158 px, fits the 224 px and 268 px state boxes of section 1.

| HA state | Without position | With position p |
|---|---|---|
| closed | Closed | Closed (position ignored) |
| open | Open | p = 100 or p missing: `Open`; otherwise `Open p%` (e.g. `Open 40%`; this is also how a stopped cover is shown, HA has no `stopped` state) |
| opening | Opening | `Opening p%` |
| closing | Closing | `Closing p%` |
| unknown | - (normal look, buttons enabled; after a tap the optimistic `Opening` / `Closing` / `Stopped` until the next fetch) | - |
| unavailable | n/a | n/a |
| loading | - | - |

Button fill follows section 1 (opening -> up filled, closing -> down filled).

### 11.4 Generic fallback for any other text state

Applies to sensor text states, vacuum/cover states not listed above, and anything
unknown:

1. `unavailable` (any case) -> `n/a`; `unknown` -> `-` with the normal look (a normal state, not unavailable).
2. Otherwise humanise: underscores to spaces, whole string lower-cased, first letter
   upper-cased (`partly_cloudy` -> `Partly cloudy`, `HEAT_COOL` -> `Heat cool`).
3. Truncate with `…` to the width of the text box (F24: 152 px landscape 1x1 box,
   224 px portrait 1x1 box, about 11 / 16 characters; 1x1 sensor/weather condition
   at F18: 152 / 224 px).
4. Empty string -> `-`.
Weather conditions keep their own short list (section 9) before this rule applies.

## 12. Sensor with attributes and details line (revision 7)

Task 030. Mockup: `docs/ui/sensor-details.svg` (portrait; `png/sensor-details.png`).
Data from `sensorView`: `main` {value, unit} (the configured attribute, else the state) and
`details` 0..2 items {label, value, unit}. A sensor without details looks exactly like
section 7. The text below only adds the **details line**. Still read-only, not focusable.

**Item text.** `Label value` + unit: one space between label and value; unit appended
without a space for `°C` / `°F` / `%`, with a space otherwise (section 7). Label = humanised
attribute name (`dew_point` -> `Dew point`, 11.4). Two items on one line are separated by a
16 px gap (no dot or bar). Text is **F18 INK**, never DIM (it changes at runtime), centred
at 1x1 and left-aligned at 2x1. F18 widths (DejaVu): `Humidity 45%` 127 px, `Dew point
9.3°C` 147 px, `Pressure 1013 hPa` about 165 px.

### 12.1 1x1 (W = 176 or 248)

The 2-text-lines rule leaves no room for value + name + details (48 + 21.6 + 21.6 + gaps >
80), so **the details line takes the name's slot and the name is not drawn** while details
exist. Same Stack B budget as section 7 (73.8 of 80); nothing moves:

| Line | Font | cy | Baseline | Box top | Box bottom |
|------|------|----|----------|---------|------------|
| 1 Value + unit | F40 / F24 unit | 39 | 53 (shared) | 15.0 | 63.0 |
| 2 Details | F18 INK | 78 | 84 | 67.2 | 88.8 |

| Element | Box (x, y, w, h) | Font / grey |
|---------|------------------|-------------|
| Value `w-N-value` | as section 7 | F40 + F24 unit, INK |
| Detail `w-N-detail` | (12, 67.2, W-24, 21.6), centred at cx = W/2 | F18 INK, one line |

Width 152 (landscape) / 224 (portrait). **Only detail 1 is shown at 1x1** (detail 2 is
ignored in both orientations; the portrait room 224 < 127 + 16 + 147 = 290 anyway).
Shortening ladder, stopping at the first step that fits:
1. `Label value unit` as is;
2. the label truncated with `…` (at least 3 characters + `…`, e.g. `Pressu… 1013 hPa`);
3. value + unit only, without label (the label is lost, values are never cut);
4. a text value (not a number) is cut with `…` to the width left after step 3.
The main value follows section 7 (wide value: one decimal fewer, then F24; never wrap).
Because the name is dropped, the user identifies the sensor by the detail label and
value; if the name matters, use 2x1.

### 12.2 2x1 (W = 364 or 508)

Value group exactly as section 7 (F56 + F40 unit, right edge x = W-12, baseline 71). Left
column = **name line + details line** (2 text lines, so the two-line name of section 7 is
not available: the name is one line, truncated with `…`):

| Line | Font | cy | Baseline | Box top | Box bottom | Gap below |
|------|------|----|----------|---------|------------|-----------|
| 1 Name | F24 INK | 36 | 44 | 21.6 | 50.4 | 6.8 |
| 2 Details | F18 INK | 68 | 74 | 57.2 | 78.8 | |
| **Sum** | | | | 21.6..78.8 | | **57.2 of 80** |

| Element | Box (x, y, w, h) | Font / grey |
|---------|------------------|-------------|
| Name `w-N-name` | (12, 21.6, C, 28.8) | F24 INK, left |
| Detail 1 `w-N-detail1` | (12, 57.2, w1, 21.6) | F18 INK, left |
| Detail 2 `w-N-detail2` | (12 + w1 + 16, 57.2, w2, 21.6) | F18 INK, left |

`C` = column width = value group left edge - 12 - 12, with the actual group width g (not
the fixed 196): `C = W - 36 - g`, at least W-232. `21.5°C` (g = 175): C = 297 (portrait)
/ 153 (landscape); `-12.5°C` (g = 195): C = 277 / 133. Portrait therefore shows both
details for `Humidity 45%` + `Dew point 9.3°C` (127 + 16 + 147 = 290 <= 297), see the
mockup. Landscape (C 133..153) shows one.
Ladder (stop at the first fit): 1. both items with labels; 2. **detail 2 dropped**, detail
1 with label; 3. detail 1 label truncated (3 characters + `…` minimum); 4. detail 1 value +
unit only. Name: truncated with `…` to C; it is never dropped at 2x1. Fill: details 2x1 left column 57.2 / 80 = 72 %; horizontal 100 %.
The column width depends on the value, so **a value change redraws the whole content box**
(12, 12, W-24, 80), not only the value box.

### 12.3 Missing, unavailable, stale

| Case | Main value | Details line |
|------|-----------|--------------|
| Detail attribute missing or `null` in an available entity | unchanged | label kept, value `-` (no unit): `Dew point -` |
| Main attribute missing (entity available) | `-` (F40 / F56 INK, no unit) | unchanged |
| Detail text value | humanised (11.4) | cut with `…` as in 12.1 step 4 |
| Loading (no data yet) | `-` | labels with `-` values (`Humidity -`), so the layout does not jump |
| Unavailable / unknown entity | `n/a`, warn badge, dotted OFF frame | **not drawn**; 1x1: the name returns in its F18 slot (OFF grey); 2x1: single name line F24 OFF at cy 52, baseline 60, badge as section 7 |
| Stale | unchanged: values stay INK, the status-bar chip carries it | unchanged |

An unavailable entity has no attributes, so showing `Humidity -` would be misleading.
Only an unavailable entity hides the details; a missing main attribute still shows them.

Redraw: 1x1 value box + detail box (12, 67.2, W-24, 21.6) only; 2x1 as 12.2. Partial
refresh; the full-refresh rules of STYLE.md section 10 apply unchanged.

### 12.4 Budget and fill

| Widget | Content stack height | Limit |
|--------|----------------------|-------|
| sensor 1x1 with details | 73.8 | 80 |
| sensor 2x1 with details, left column | 57.2 (value 67.2) | 80 |
