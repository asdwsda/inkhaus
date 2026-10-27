# Widget specs

All coordinates are **widget-relative** `(x, y, w, h)`, origin at the widget's
top-left. `W` and `H` are the widget's pixel size (STYLE.md section 4):
W = 246 / 508 (portrait, 1 / 2 cells), 180 / 376 / 572 / 768 (landscape, 1 / 2 /
3 / 4 cells); H = 144 (1 row) or 304 (2 rows). Fonts, greys, spacing, button
formula, badge slot and state looks come from STYLE.md. Sizes and clamping:
layout.md section 3.

Common to all widgets:
- Frame: rect (0, 0, W, H), radius 8, border 2 INK, fill PAPER (INK when "on").
- Badge slot: (W-32, 8, 24, 24): hourglass (pending), alert (failed), warn
  (unavailable), else empty.
- Text colour: INK on PAPER, PAPER on INK; OFF grey when stale, loading or
  unavailable. Names come from config `name`, else HA `friendly_name`.
- "Stale" is global (status-bar chip). A stale widget differs only by OFF-grey
  value text.
- Redraw: partial, only the listed element rects (STYLE.md section 10).
- Button-row formula (n buttons): `bw = floor((W-16-16(n-1))/n)`, start
  `x0 = 8 + floor(rem/2)`, button i at `x0 + i*(bw+16)`.
  For n = 3: portrait W=508 -> bw 153, x = 8, 177, 346; landscape W=376 -> bw 109,
  x = 8, 133, 258; W=246 -> bw 71 (too small; 3-button widgets are never 1 cell wide).

## 1. Cover (single shutter)

HA: entity domain `cover`; state (`open`, `closed`, `opening`, `closing`,
`unavailable`); attribute `current_position` (0..100); `friendly_name`.
Services (004): `cover.open_cover`, `cover.stop_cover`, `cover.close_cover`.
Sizes: 2x1 (default) up to 4x2.

### 2x1 layout (H = 144)

| Element | Rect | Font / grey | Content |
|---------|------|-------------|---------|
| Name | (8, 8, W-176, 32) | F24 INK, left | name (fit rule STYLE.md section 2) |
| Status text | (W-160, 8, 120, 32) | F18 INK, right-aligned at W-40 | see below |
| Badge | (W-32, 8, 24, 24) | S icon | pending/failed/unavailable |
| Button 1 "up" | (x0, 56, bw, 80) | glyph M | sub-region 1 |
| Button 2 "stop" | (x0+bw+16, 56, bw, 80) | glyph M | sub-region 2 |
| Button 3 "down" | (x0+2(bw+16), 56, bw, 80) | glyph M | sub-region 3 |

Status text: `closed` -> `Closed`; `open` with position p -> `p%` (or `Open` if p
is missing); `opening` -> `Opening p%`; `closing` -> `Closing p%` (omit `p%` if
missing). Unavailable -> `n/a`; loading -> `-`.

### 2x2 layout (H = 304; adds position display)

Name, status text, badge as above. Then:
- Position value (8, 48, W-16, 72), F56 centre: `p%` or `-`.
- Position bar (8, 128, W-16, 24): 2 px INK outline, TRACK fill, INK fill from
  the left, width `round((W-20) * p / 100)`; shown only if p is known.
- State word (8, 160, W-16, 32), F24 DIM centre: `Open`, `Closed`, `Opening`,
  `Closing`.
- Buttons row at y = 216 (instead of 56): (x_i, 216, bw, 80).

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
| Pending | tapped button: dashed 3 px border, hourglass M instead of glyph; other buttons unchanged. Confirmed when the next fetch shows the matching state or position change (or after 2 fetches without change -> Failed) |
| Failed | alert badge; buttons normal |
| Unavailable | dotted OFF frame and buttons, OFF glyphs, warn badge, status `n/a` |
| Loading | status `-`, buttons normal |
| Stale | status text OFF grey |

Redraw: status text rect (and position elements in 2x2); the two affected
buttons when the fill moves (old and new); badge rect; the tapped button on
pending. Rocker INNER focus: tapped button border.

## 2. Cover group

HA: members' states and `current_position` (fetched once per distinct entity,
shared with single-cover widgets); name is required in config (`name`).
Services: the same three, with `entity_id` listing **all member ids** (004).
Layout, sub-regions, sizes and actions: identical to Cover.

Aggregation for the status text:
1. any member `opening` -> `Opening`; any `closing` -> `Closing` (position
   omitted);
2. else all `closed` -> `Closed`;
3. else all available members have the same `current_position` p -> `p%`;
4. else `Mixed`.

Button fill: `Opening` -> up filled, `Closing` -> down filled.
Unavailable only if **all** members are unavailable. If some members are
unavailable: status keeps going from available ones and the badge shows the
alert icon (not warn); controls stay active.

## 3. Light

HA: domain `light`; state `on`/`off`; attribute `brightness` (0..255,
percent = round(b*100/255)); `friendly_name`. Service: `light.toggle`.
Sizes: 1x1 (default), 2x1.

| Element | Rect | Font / grey |
|---------|------|-------------|
| Icon bulb (M) | (12, 12, 40, 40) | INK, or PAPER on INK |
| State text | (60, 12, W-100, 40) | F24, left |
| Name | (12, 64, W-24, 68) | F24, top, max 2 lines, line height 28 |
| Badge | (W-32, 8, 24, 24) | S |

State text: off -> `Off`; on -> `On`, and in 2x1 (W >= 376) `On 80%` when
brightness is known. The bulb icon: outline when off, solid-filled body when on.

Sub-regions: none. The whole widget rect is the toggle (id 0). Action: toggle.

| State | Look |
|-------|------|
| Off | PAPER, solid border |
| On | **INK fill**, PAPER content |
| Pending | flips to the requested fill at once; dashed 3 px border; hourglass badge |
| Failed | real state; alert badge |
| Unavailable | dotted OFF frame, OFF content, state text `n/a`, warn badge, taps ignored |
| Loading | state text `-`, PAPER fill |
| Stale | name stays; state text OFF grey (on INK fill: no change, the status-bar chip carries it) |

Redraw: the whole widget rect on any change (small, one region). Brightness-only
change: state text rect.

## 4. Switch

HA: domain `switch`; state `on`/`off`. Service: `switch.toggle`.
Same layout, states and redraw as Light with the plug icon; no brightness.
Sizes 1x1 (default), 2x1.

## 5. Automation

HA: domain `automation`; state `on` (enabled) / `off` (disabled).
Services: `automation.turn_off` when on, `automation.turn_on` when off
(004). Layout as Light with the auto icon; state text `Enabled` / `Disabled`
(F24; in a 180 px wide widget the text rect is 80 px, so it fits by the
text fit rule stepping down to F18). On = INK fill. Pending/failed/unavailable
identical to Light. Sizes 1x1 (default), 2x1.

## 6. Climate (thermostat)

HA: domain `climate`; state (= hvac mode); attributes `current_temperature`,
`temperature` (target), `current_humidity`, `hvac_mode`, `hvac_action`,
`min_temp`, `max_temp`, `target_temp_step`, `friendly_name`.
Service: `climate.set_temperature` (clamped to min/max, rounded to step by
the action builder, 004). Sizes 2x2 (default), up to 4x2; the layout is fixed
height 304 and the elements are anchored left/right as given.

| Element | Rect | Font / grey | Content |
|---------|------|-------------|---------|
| Name | (8, 8, W-48, 32) | F24 INK | name |
| Badge | (W-32, 8, 24, 24) | S | |
| Current temperature | (8, 48, W-136, 64) | F56 INK left; unit F40 | `21.5` + `°C` (unit from HA config, default `°C`) |
| Humidity icon | (W-120, 52, 24, 24) | drop S | only if `current_humidity` known |
| Humidity text | (W-92, 48, 84, 32) | F24 right | `45%` |
| Action text | (W-120, 80, 112, 28) | F18 DIM right | `Heating` / `Cooling` / `Idle` / `Off` from `hvac_action` (mode if missing) |
| Button "-" | (8, 120, 80, 80) | minus M | sub-region 1 |
| Button "+" | (W-88, 120, 80, 80) | plus M | sub-region 2 |
| Target label | (96, 120, W-192, 24) | F18 DIM centre | `Target` |
| Target value | (96, 144, W-192, 56) | F40 INK centre | `22.0°` (decimals: 1 if step < 1 else 0) |
| Mode row | (8, 216, W-16, 80) | see below | sub-region 3 |

Mode row, read-only (default): no border; a 1 px OFF rule on its top edge, mode
icon M at (24, 236, 40, 40) (flame/snow/auto/power/fan/drop for
`heat`/`cool`/`heat_cool`+`auto`/`off`/`fan_only`/`dry`) and text
`Mode: Heat` at (76, 216, W-92, 80), F24, left. No hit region.
OPEN (needs architect): changing the mode requires a `climate.set_hvac_mode`
action and the `hvac_modes` attribute, neither in task 004. If added, the mode
row becomes a normal button (2 px border, whole row = sub-region 3) and each
tap cycles to the next mode in `hvac_modes` (pending, then confirmed).

Actions:

| Sub-region | Action |
|------------|--------|
| 1 "-" | target := target - step (min_temp bound) |
| 2 "+" | target := target + step (max_temp bound) |
| 3 mode | only if enabled (see OPEN) |

Taps change the displayed target at once (pending look on the value rect);
the service call is sent once, 1.5 s after the last tap or immediately when the
session ends. Reaching min/max leaves the value unchanged (no pending).

States:

| State | Look |
|-------|------|
| Normal | as above; `hvac_mode off` shows target `Off` (F40) and the action text `Off`; buttons still active |
| Pending | target value: dashed 3 px underline (96, 196, W-192, 3) and hourglass badge; value is the requested target |
| Failed | real target; alert badge |
| Unavailable | dotted OFF frame, buttons dotted, temperatures `n/a` (F40 OFF), warn badge, no humidity |
| Loading | temperatures `-`; buttons active (a tap needs a known target, so it is ignored until the first value) |
| Stale | temperatures OFF grey |

Redraw: current temperature rect; humidity icon+text; action text; target value
rect (+ label unchanged); mode row; badge. A target tap redraws only target
value rect and badge.

## 7. Sensor

HA: domain `sensor`; state (numeric or text); attributes
`unit_of_measurement`, `device_class`; `friendly_name`. Read-only.
Sizes 1x1 (default), 2x1. Not focusable, no hit regions.

| Element | Rect | Font / grey |
|---------|------|-------------|
| Value + unit | (8, 12, W-16, 64) | value F40, unit F24 (F56 / F40 when W >= 376), centre |
| Name | (8, 92, W-16, 44) | F18 DIM, centre, max 2 lines (F24 single line when W >= 376, rect (8, 88, W-16, 40)) |
| Badge | (W-32, 8, 24, 24) | S |

Value formatting: numeric states rounded to 1 decimal (drop `.0`); non-numeric
text uses F24, centred, max 2 lines. Unit appended without space for `°C`/`%`,
with a space otherwise.

| State | Look |
|-------|------|
| Normal | INK |
| Unavailable | value `n/a` in OFF, warn badge, dotted OFF frame |
| Loading | value `-` OFF |
| Stale | value OFF grey |

Redraw: value rect only. Pending: n/a.

## 8. Binary sensor

HA: domain `binary_sensor`; state `on`/`off`; `friendly_name`. Read-only, 1x1
(default) / 2x1, not focusable.
OPEN (needs architect): the 004 attribute list has no `device_class` for
binary_sensor, so wording is generic (`On`/`Off`); adding `device_class` would
allow `Open`/`Closed`, `Detected`/`Clear`.

| Element | Rect | Content |
|---------|------|---------|
| Indicator | (W/2-20, 16, 40, 40) | circle d=40: INK-filled = on, 2 px outline = off |
| State text | (8, 64, W-16, 28) | F24 centre: `On` / `Off` |
| Name | (8, 96, W-16, 44) | F18 DIM, centre, max 2 lines |
| Badge | (W-32, 8, 24, 24) | S |

Unavailable: dotted OFF frame, indicator drawn as warn icon (40), text `n/a`.
Redraw: indicator + state text rect (8, 16, W-16, 76).

## 9. Weather

HA: domain `weather`; state = condition; attributes `temperature`, `humidity`,
`temperature_unit`; `friendly_name`. Read-only. Sizes 2x1 (default), 1x1 to
4x1. Not focusable.

### 2x1 and wider (H = 144)

| Element | Rect | Font / grey |
|---------|------|-------------|
| Name | (8, 8, W-48, 32) | F24 INK |
| Badge | (W-32, 8, 24, 24) | S |
| Condition icon | (12, 56, 64, 64) | L |
| Temperature | (88, 52, 170, 72) | F56 left, unit F40: `12°C` (integer) |
| Humidity icon | (W-112, 56, 24, 24) | drop S |
| Humidity text | (W-84, 52, 76, 32) | F24 right: `76%` |
| Condition text | (W-120, 92, 112, 28) | F18 DIM right |

Condition text (short): Sunny, Clear, Cloudy, Partly, Rain, Pouring, Snow, Fog,
Wind, Storm, else the raw state truncated.
Humidity is omitted if unknown.

### 1x1 (H = 144)

Name (8, 8, W-48, 28) F18 DIM; icon M (12, 44, 40, 40); temperature
(56, 40, W-64, 56) F40 left, unit F24; condition text (8, 108, W-16, 28) F18
DIM centre. No humidity.

States: Unavailable: icon `unknown` OFF, temperature `n/a`, dotted frame, warn
badge. Loading: `-`. Stale: temperature OFF grey. Redraw: icon + temperature +
humidity + condition rects individually.

## 10. Vacuum

HA: domain `vacuum`; state (`cleaning`, `docked`, `idle`, `paused`,
`returning`, `error`, `unavailable`); attributes `battery_level` (0..100),
`status` (text, used only for the error text), `friendly_name`.
Services (004): `vacuum.start`, `vacuum.pause`, `vacuum.return_to_base`.
Sizes 2x1 (default) up to 4x2.

### 2x1 (H = 144)

| Element | Rect | Font / grey |
|---------|------|-------------|
| Name | (8, 8, W-232, 32) | F24 INK |
| Status text | (W-216, 8, 108, 32) | F18 INK, right: `Cleaning`, `Docked`, `Idle`, `Paused`, `Returning`, `Error` |
| Battery icon | (W-104, 12, 24, 24) | S battery (fill = pct) |
| Battery text | (W-76, 8, 40, 32) | F18 right: `85%` |
| Badge | (W-32, 8, 24, 24) | S |
| Button 1 "start" | (x_0, 56, bw, 80) | play M, sub-region 1 |
| Button 2 "pause" | (x_1, 56, bw, 80) | pause M, sub-region 2 |
| Button 3 "dock" | (x_2, 56, bw, 80) | dock M, sub-region 3 |

2x2 (H = 304): status big (8, 48, W-16, 56) F40 centre; battery bar
(8, 112, W-16, 24) as the cover position bar with the percent text
(8, 144, W-16, 32) F24 centre; buttons at y = 216. Name, status, badge as in
2x1 are replaced by: name (8, 8, W-48, 32); no top-row status/battery.

Actions: 1 -> `vacuum.start`; 2 -> `vacuum.pause`; 3 -> `vacuum.return_to_base`;
0 -> none. All -> pending.

Fill (current mode): `cleaning` -> start filled; `paused` -> pause filled;
`docked` or `returning` -> dock filled; `idle`/`error` -> none. `error` shows
the `status` text in the status rect (truncated) and the alert badge.

States: Pending (tapped button dashed + hourglass M, confirmed by the next
fetch with a different state); Unavailable (dotted, `n/a`, battery `-`, buttons
dotted); Loading (`-`); Stale (status/battery text OFF grey). Battery <= 15 %:
battery icon with the "!" glyph (as status bar).

Redraw: status text, battery icon+text, affected buttons, badge.

## Summary table

| Widget | Sub-regions (id) | Focusable | Read-only |
|--------|------------------|-----------|-----------|
| cover / group | 1 up, 2 stop, 3 down | yes (INNER mode, entry = stop) | no |
| light, switch, automation | whole widget = id 0 | yes (push = action) | no |
| climate | 1 minus, 2 plus, 3 mode (optional) | yes (INNER, entry = minus) | no |
| sensor, binary_sensor, weather | none | no | yes |
| vacuum | 1 start, 2 pause, 3 dock | yes (INNER, entry = start) | no |

Sub-region rects for the engine (`subRegions`): cover/group/vacuum: three
buttons from the button-row formula at y = 56 (H=144) or y = 216 (H=304);
climate: minus (8,120,80,80), plus (W-88,120,80,80), mode (8,216,W-16,80) only
when enabled.
