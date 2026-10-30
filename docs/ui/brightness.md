# Brightness popup (dimmable lights)

Task: docs/tasks/045-brightness-popup.md. Mockups: `brightness-portrait.svg`, `brightness-landscape.svg`
(PNGs in docs/ui/png/). Shared values come from STYLE.md (revision 5): greys INK/PAPER only in this
popup (dynamic text is INK), fonts F18/F24/F40, 64 x 64 targets with 12 px gaps, centre-line rule,
border 2, radius 8.

## 1. Purpose and entities

A modal panel over the tab, for one `light` entity that is dimmable (`supported_color_modes` has a
brightness mode, or the `brightness` attribute is present). Reads `state`, `attributes.brightness`
(0..255, percent = round(b * 100 / 255)), `friendly_name`. Writes `light.turn_on` with
`brightness_pct` (1..100) and `light.turn_off` (for 0 / Off).

Open: long press on the light widget, or rocker long push on a focused dimmable light. A short tap stays
on/off. Not available for non-dimmable lights (long press does nothing).

## 2. Popup rules (new; STYLE.md is not edited, the architect may fold these in)

1. **The tab underneath is unchanged**: no dimming. A grey dither would be slow (quality mode) and
   would look dirty. The panel stands out by a 3 px INK border plus a **6 px solid INK shadow**
   (right and bottom), radius 8, PAPER fill. Status bar / tab bar stay as they are, but are inactive.
2. **Modal**: while open, every touch outside the panel only closes it (the tap is swallowed, it does
   not toggle a widget or switch a tab). Rocker events go to the popup.
3. **Rocker has no focus ring** in the popup: the rocker acts directly (section 5), so nothing is focused.
4. New icon `close` (M, 40): a bold X (two diagonals, stroke 3). Add to STYLE.md section 8, Controls,
   when the popup is approved.
5. The popup is closed (and redrawn away) before the device sleeps, so the frozen screen never shows it.

## 3. Layout

Panel-relative coordinates in the tables; absolute rects are given too. Panel padding 16, rows 12 apart.
All targets 64 high. Text boxes follow the 1.2 rule; every element sits inside the panel minus 12 px.

### 3.1 Portrait (540 x 960)

Panel (16, 278, 508, 380), centred in the content area (68..868). Shadow rect (22, 284, 508, 380).
Dirty rect of the whole popup = (16, 278, 514, 386). There is no Done button; the close button is the
only explicit close control.

| Element | Rect (absolute) | Font / icon | Grey | Notes |
|---------|-----------------|-------------|------|-------|
| Panel frame | (16, 278, 508, 380) | border 3, r 8 | INK on PAPER | `w-1-frame` |
| Name | x = 32, cy = 326, left, max width 380 (to x 412) | F24 | INK | truncated with "..." |
| Close button | (444, 294, 64, 64) | `close` M | INK, outline | closes, sends pending value |
| Percent | centred at x = 270, cy = 390, box (200, 366, 140, 48) | `60` F40 + `%` F24 sharing the baseline y = 404 (unit dx 2) | INK | when off: `Off` F40. Region (190, 362, 160, 56) |
| Pending icon | (484, 378, 24, 24), cy = 390 | `hourglass` S / `alert` S | INK | only while a change is unsent or unconfirmed; blank otherwise |
| Bar (tap target) | (32, 426, 476, 64) | border 2, r 8 | INK on PAPER | |
| Bar fill | (36, 430, round(468 * pct / 100), 56), r 4 | | INK | empty bar at 0 |
| Step minus | (32, 502, 150, 64) | `minus` M + `10 %` F24, group centred (icon 40, 8 px, text) | INK, outline | |
| Off | (194, 502, 152, 64) | `Off` F24 centred | | filled when off |
| Step plus | (358, 502, 150, 64) | `plus` M + `10 %` F24 | INK, outline | |
| Preset 25 | (32, 578, 110, 64) | `25%` F24 centred | | |
| Preset 50 | (154, 578, 110, 64) | `50%` | | |
| Preset 75 | (276, 578, 110, 64) | `75%` | | |
| Preset 100 | (398, 578, 110, 64) | `100%` | | |

Buttons: outline (PAPER, INK border 2). **Filled (INK, PAPER text) means "current"**: the preset
equal to the current percent, and Off when the light is off. Disabled: Minus at 0 % and Plus at 100 %
get the unavailable style (dotted OFF border, OFF glyph, no action). Mockup state: 60 % (no preset
filled). Off moved into the step row (Minus | Off | Plus) to free the old last row; the panel is 76 px
shorter.

### 3.2 Landscape (960 x 540)

Panel (152, 136, 656, 324), centred in the content area (72..524). Shadow (158, 142, 656, 324).
Dirty rect of the whole popup = (152, 136, 662, 330). The two-row top block is compressed: percent
and bar share a row.

| Element | Rect (absolute) | Font / icon | Notes |
|---------|-----------------|-------------|-------|
| Panel frame | (152, 136, 656, 324) | border 3, r 8 | |
| Name | x = 168, cy = 184, max width 540 | F24 INK | |
| Close button | (728, 152, 64, 64) | `close` M | |
| Percent | x = 168 left, cy = 260, box (168, 236, 116, 48) | `75` F40 + `%` F24 on one baseline (y = 274) | `Off` when off. Region (160, 228, 130, 64) |
| Pending icon | (284, 248, 24, 24), cy = 260 | `hourglass` / `alert` S | |
| Bar | (324, 228, 468, 64), fill (328, 232, round(460 * pct / 100), 56) | | |
| Step minus | (168, 304, 200, 64) | `minus` M + `10 %` | |
| Step plus | (380, 304, 200, 64) | `plus` M + `10 %` | |
| Off | (592, 304, 200, 64) | `Off` F24 | filled when off |
| Preset 25 / 50 / 75 / 100 | (168, 380, 147, 64), (327, 380, 147, 64), (486, 380, 147, 64), (645, 380, 147, 64) | F24 | the current one filled (mockup: 75 %) |

Checks: portrait 16 + 476 + 16 = 508; 3 * 12 + 4 * 110 = 476; 150 + 12 + 152 + 12 + 150 = 476; vertical
16 + 64 + 8 + 48 (percent) + 12 + 64 + 12 + 64 + 12 + 64 + 16 = 380 (percent box sits 8 below the close
button, bar 12 below the box). Landscape 16 + 624 + 16 = 656; 3 * 200 + 2 * 12 = 624; 4 * 147 + 3 * 12 =
624; vertical 16 + 64 + 12 + 64 + 12 + 64 + 12 + 64 + 16 = 324.

## 4. Value rules

- Bar: tap at x sets `pct = clamp(round((x - bar_x) / bar_w * 100), 1, 100)`. No dragging (e-ink). The
  Off button is the only way to 0.
- Step: pct = clamp(pct + / - 10, 0, 100); from Off, Plus gives 10. Minus at 10 gives 0 (= Off).
- Presets set 25 / 50 / 75 / 100. Off sets 0.
- Every input updates the shown value at once (optimistic) and restarts a 400 ms debounce; after 400 ms
  without input the last value is sent (`brightness_pct`, or `light.turn_off` for 0). Hourglass shows
  from the first input until the state is confirmed by the next fetch; a failed send shows `alert`
  until close; on close the real state is drawn with the usual failed badge on the widget.
- Close on the close button, a tap outside, rocker push, or 10 s without input. Closing with an unsent
  value sends it immediately (no wait for the debounce).

## 5. Interactions

| Input | Action | Redraw (fast partial, region = rect) |
|-------|--------|---------------------------|
| Long press on a dimmable light / rocker long push on it | open popup at the light's current percent | panel + shadow (one refresh) |
| Tap bar | set pct from x | percent rect, bar rect, preset buttons that change state, Off button if it changes, pending icon |
| Tap Minus / Plus / preset / Off | set value | same set as above (+ the tapped button briefly needs nothing extra: no pressed state) |
| Tap close button, outside | close (sends pending value) | panel + shadow, plus the light widget rect (new "On 60%") |
| 10 s inactivity | same as close button | same |
| Rocker UP / DOWN | + 10 % / - 10 % (UP brighter) | as for Plus / Minus |
| Rocker PUSH | close (sends pending value) | as close |
| Rocker long push | close | as close |

Rocker order note: task 045 wrote "up/down = -/+10 %"; this spec uses UP = +10 (brighter), which is the
natural reading and matches the cover widget. Decided: portrait UP = brighter; landscape the key pointing right (BtnC) = brighter.

## 6. Refresh

- **Open**: one refresh of the panel + shadow region. The region holds the tab's greys (DIM names, OFF
  hairlines) after a GC16, so it is one `epd_quality` refresh (STYLE section 10); no full refresh.
- **Value changes**: `epd_fastest` on the small rects above only (everything in the popup is pure
  black/white, so no quality pass). The percent rect and the bar rect are two separate rects; preset
  and Off buttons only when their fill changes. If the changed rects exceed 60 % of the popup's inner
  area, use the panel rect.
- **Close**: the panel + shadow region and the light widget rect, one `epd_quality` refresh (the
  region contains the tab's greys again). No full refresh; the normal ghost debt rule (12 feedback
  refreshes, 150 %) counts popup refreshes and may run a GC16 at the next quiet moment after close.
- Never a full refresh while the popup is open.

## 6b. States

| State | Behaviour |
|-------|-----------|
| Light off when opened | percent shows `Off`, bar empty, Off filled, Minus disabled |
| Loading (no brightness yet, state on) | percent `-`, bar empty; controls active |
| HA unreachable / send fails | optimistic value stays, `alert` S in the pending slot until close; widget shows the real state afterwards |
| Light becomes unavailable while open | popup closes (quality refresh of the region), widget in unavailable style |
| Stale data | no marker in the popup (status bar clock icon is covered by the same rules; the popup does not hide the status bar) |
| Fetch result while open | applied to the popup only when no change is unsent |

Mockup ids: panel `w-1` / `w-1-frame`, every element `w-1-*`, buttons `*-btn`; the background cells are
not widgets (`bg-*`). check_mockups: 0 problems.
