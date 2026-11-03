# New widgets (proposal, 2026-11-03)

Status: **accepted** by the user 2026-11-03 (open decisions 1-6 taken with the recommended defaults); media widget gets album art in a follow-up revision. Mockups in `docs/ui/`, PNGs in `docs/ui/png/`;
STYLE.md, widgets.md and layout.md are unchanged. Everything reuses their rules: grid (STYLE 4), inner grid and stacks (4b),
text boxes 1.2 x font size, centre-line rule, only F18/F24/F40/F56, dynamic text INK, `name`/`state` ids for the checker, visual
states (STYLE 9), refresh rules (STYLE 10), rocker yes/no (STYLE 10b), popup rules (brightness.md 2: modal, 3 px border, 6 px
shadow, `close` icon, closed before sleep) and the history popup's refresh/open/close rules (history.md 1, 7).
All widget coordinates are widget-relative `(x, y, w, h)`; `W` = 176 / 364 / 552 landscape, 248 / 508 portrait.

Mockups (all landscape unless noted; generated, spec == picture):
`widgets-new-tab-landscape.svg`, `widgets-new-tab-portrait.svg` (normal state in the grid),
`widgets-new-states-small.svg` (1x1: normal, attention, pending, loading, unavailable),
`widgets-new-states-wide.svg` (weather, calendar, media, energy: normal, alternative, loading, unavailable),
`widgets-new-popup-notifications.svg`, `-popup-alarm.svg`, `-popup-media.svg`, `-popup-confirm.svg`.

## 0. Shared rules (new)

- **Filled = needs attention** (a new reading of "on"): notifications with count > 0, alarm triggered, lock unlocked, garage
  open. Everything else is an outline; armed vs disarmed alarm and locked vs unlocked are also told apart by icon shape and text,
  never by grey. (Alarm armed is *not* filled: a quiet armed house is the normal case.)
- Widgets that open a popup or dialog on tap are focusable; rocker PUSH = the same action as a tap. Read-only widgets
  (weather, calendar, energy) are not focusable.
- Pending, failed, unavailable, loading, stale: STYLE 9 unchanged. Pending on an INK-filled widget draws the 3 px dashed border as
  a PAPER dashed line inset 5 px (a black dashed line on a black fill would be invisible); see lock/garage pending in
  `widgets-new-states-small.svg`.
- Text caps for 1x1 toggle-layout state text: 7 characters (widgets.md 11).
- New icons (24 grid, stroke 2, to be added to STYLE 8 when accepted): `bell` (M/L, solid), `shield` (solid), `shield_off`
  (outline), `shield_alert` (solid, "!" cut out), `lock` (solid body, closed shackle), `lock_open` (outline body, raised
  shackle), `garage_on` (house outline, filled door gap = door raised), `garage_off` (house outline, slat lines), `prev`, `next`
  (bar + triangle), `volume` (speaker + two waves), `mute` (speaker + X), `backspace`, `chevron_left/right`, small weather glyphs at
  S (sunny, partlycloudy, cloudy, rainy; snowy/fog/windy/lightning/pouring still to draw at S), `close` (existing).
  `play`, `pause`, `power`, `minus`, `plus`, `hourglass`, `alert`, `warn` are existing.

## 1. Notifications, 1x1

**HA:** WebSocket `persistent_notification/get` (list of `notification_id`, `title`, `message`, `created_at`); dismiss with WS
`persistent_notification/dismiss {notification_id}`. No entity. Fetched with every state fetch (one extra WS call). Config:
`type: notifications`, optional `name` (default `Alerts`).

| Element | Box | Font / grey | Content |
|---|---|---|---|
| Icon `w-N-icon` | (12, 28, 48, 48) | L `bell`, INK (PAPER on fill) | |
| Count `w-N-count` | starts x = 64, cy 39, baseline 53, box (64, 15, W-76, 48) | F40 INK | `0`..`99`, `99+`; `-` loading, `n/a` unavailable |
| Name `w-N-name` | (64, 67.2, W-76, 21.6), cy 78, baseline 84 | F18 DIM (static caption; PAPER on fill; OFF unavailable) | `Alerts`, truncated with `…` |
| Badge | (W-36, 12, 24, 24) | S | unavailable only (short `n/a` never reaches it) |

Budget = Stack B: count 15.0..63.0, name 67.2..88.8, gap 4.2, 73.8 of 80. Fill: horizontal 100 % reserved, vertical 92 %.
Look: count 0 -> outline; count > 0 -> **INK fill**, PAPER content. Pending/failed: not applicable (read-only count).
Unavailable (WS not connected and nothing cached): dotted OFF frame, bell OFF, `n/a`, warn badge. Loading: `-`.
**Redraw:** count box (and the whole widget when fill flips). **Interactions:** tap / PUSH opens the list popup (also at count 0).

### 1.1 Notification list popup

Mockup `widgets-new-popup-notifications.svg`. Panel (40, 94, 880, 408), shadow (46, 100, 880, 408), border 3, radius 8;
dirty rect (38, 92, 890, 418). Frame id `w-90-frame`.

| Element | Landscape rect / position | Font / grey |
|---|---|---|
| Title `Notifications (4)` | x 56, cy 142 | F24 INK |
| `Clear all` button | (520, 110, 152, 64), label centred cy 142 | F24 INK, outline |
| Prev / next page | (688, 110, 64, 64) / (764, 110, 64, 64), `chevron_left/right` M | outline; at the first / last page dotted OFF, inert |
| Close | (840, 110, 64, 64), `close` M | outline |
| Row i (0..2) frame | (56, 190 + 88 i, 848, 76), border 2, r 8 | outline |
| Row title | x 72, cy row+25 (box 10.6..39.4), max width 650 | F24 INK, `…`; empty title -> `Notification` |
| Row time | right edge 812, cy row+25 | F18 INK: `13:58` today, `3.11.` earlier |
| Row message | x 72, cy row+55 (box 44.2..65.8), max width 730 | F18 INK, one line, markup stripped, `…` |
| Row dismiss | (828, row+6, 64, 64), `close` glyph 32 | outline |
| Footer count | x 56, cy 470 | F18 INK `1-3 of 4` |
| Footer hint | right edge 904, cy 470 | F18 DIM static |

Rows per page: 3. **Portrait** (not mocked): panel (16, 116, 508, 704); panel-relative: title x 16 cy 48; prev (276, 16, 64, 64),
next (352, 16), close (428, 16); rows y = 96 + 88 i, i < 6, row (16, y, 476, 76), title x 32 max width 290, time right edge 384
(cy y+25), message x 32 max width 352, dismiss (400, y+6, 64, 64); `Clear all` (16, 628, 152, 64); footer count x 184, cy 660.

**Interactions.** Touch: row dismiss -> one WS dismiss, the button shows an hourglass M until the call returns, then the row is
removed and the list below moves up (failed: `alert` glyph in the button, row stays). `Clear all` -> yes/no dialog
(section 7, title `Dismiss all 4?`) then one dismiss per id. Chevrons change page; close, tap outside the panel, or rocker long push
close. **Rocker:** UP / DOWN move a 3 px focus ring (drawn 1..4 px outside, the row padding is 6 px) over the dismiss buttons of the
visible rows, then `Clear all`; at the last row the next page opens and focus continues; PUSH = activate the focused control;
long push = close. **Empty list:** F24 `No notifications` centred in the list area, `Clear all` and chevrons dotted. **Loading:**
rows area shows `Loading` (F24). **Offline:** the cached list is shown, dismiss is queued like other commands (pending glyph).
**Refresh:** open and close = one `epd_quality` partial of the dirty rect (the tab beneath holds greys); inside, a dismiss redraws
rows area (56, 190, 848, 252) + footer + title with `epd_fastest` (INK/PAPER only).

## 2. Weather with forecast, wide

Extends the weather widget (widgets.md 9) when `forecast: true` is set: size wide only (portrait 508, landscape 552), the 2x1
and 1x1 layouts are unchanged. **HA:** `weather.*` state (condition), `temperature`, `temperature_unit` (as today) plus the service
`weather.get_forecasts {type: daily}` with response (WS `call_service` + `return_response`; the `forecast` attribute no longer
exists); first entries of today are skipped, the next 3 days are shown. Forecast is fetched with the state fetch, cached.
Mockup: `widgets-new-tab-*.svg`, `widgets-new-states-wide.svg`.

Left block = Stack B beside the L icon (as weather 1x1, widgets.md 9); right block = 3 forecast columns.
`cw` = column width = 108 (landscape) / 100 (portrait); `fx` = forecast start = W - 12 - 3 cw (216 / 196).

| Element | Box | Font / grey |
|---|---|---|
| Icon `w-N-icon` | (12, 28, 48, 48) | L, INK |
| Temperature `w-N-temp` | x 68, cy 39, baseline 53, `12°C` (F40 value + F24 unit, shared baseline) | INK |
| Condition `w-N-cond` | x 68, cy 78, baseline 84, max width fx - 16 - 68 | F18 INK, short list of widgets.md 9, `…` |
| Divider `w-N-div` | (fx - 8, 20, 1, 64) | OFF hairline, static |
| Day `w-N-dI` (I = 0..2) | centred at cx = fx + cw I + cw/2, cy 24 (box 13.2..34.8) | F18 INK `Tue` |
| Forecast icon `w-N-iI` | (cx - 12, 40, 24, 24) | S, INK |
| High/low `w-N-tI` | centred at cx, cy 80 (box 69.2..90.8) | F18 INK `14°/6°` |

Budget: forecast columns 13.2..90.8 = 77.6 of 80 (day box, 5.2 px, icon 24, 5.2 px, hi/lo box); left block 73.8; gaps 5.2 / 5.2 / 4.2.
Fill: horizontal 100 %, vertical 97 % (columns). Widths: `14°/6°` about 62 px in a 100 px column; `-12°C` ends at x = 164 < 180 (portrait limit).
**States.** Unavailable: dotted frame, `unknown` OFF icon, `n/a` F40 right-aligned at W-13 (baseline 66), warn badge (68, 57), no forecast.
Loading: icon `unknown`, temperature `-`, columns `-`/`-`. **Forecast missing** (call failed, current fine): current block normal,
day names kept, icons dropped, hi/lo `-` (see "Alternative" row). **Read-only, not focusable, no tap.** Redraw: temperature, condition, each
column individually (merged into the widget rect above 60 %).

## 3. Calendar, 2x1 (or wide)

**HA:** `calendar.*`, service `calendar.get_events {duration: 7 days}` with response; events that have not ended, sorted by start,
the first two are shown. Config `entity` (one calendar; several via a later `entities:` list, out of scope), optional `name`
(not drawn; the event titles identify it). Sizes: 2x1 (364 / 508) default, wide also fine (same boxes, wider titles); no 1x1.

| Element | Box | Font / grey |
|---|---|---|
| Time 1 `w-N-time1` | x 12, cy 36 | F18 INK |
| Time 2 `w-N-time2` | x 12, cy 69 | F18 INK |
| Title 1 `w-N-title1` | x 118, cy 36 (box 21.6..50.4), max width W-130 | F24 INK, `…` |
| Title 2 `w-N-title2` | x 118, cy 69 (box 54.6..83.4), max width W-130 | F24 INK, `…` |

Stack A (61.8 of 80); time and title of a row share the centre line. Time column 12..112 (`Tue 09:00` = 97 px). Time text: today
`14:30`, running `now`, later day `Tue 09:00`, all day today `all day`, all day later `Tue`. Landscape title width 234 px (about 17
characters), portrait 378. **Empty:** `No events` F24 (cy 36) + `next 7 days` F18 (cy 69). Loading: `-` in both time slots. Unavailable:
`n/a` F24 (cy 52), warn badge (W-36, 12), dotted frame. Fill: horizontal 100 % reserved, vertical 77 %. Read-only, not focusable.
Redraw: whole widget (events shift as a block).

## 4. Media player, wide

**HA:** `media_player.*`; `state` (`playing`, `paused`, `idle`, `on`, `standby`, `buffering`, `off`), attributes `media_title`,
`media_artist`, `media_album_name`, `volume_level`, `is_volume_muted`, `supported_features`. Services: `media_play_pause`,
`media_previous_track`, `media_next_track`, `turn_on` (from `off`), `volume_set`, `volume_mute`. Size: wide only (508 / 552).
Layout = cover widget (widgets.md 1): Stack A on the left, 3-button row on the right.

| Element | Box | Font / grey |
|---|---|---|
| Line 1 `w-N-l1` | (12, 21.6, W-252, 28.8), cy 36 | F24 INK: `media_title`, else the name; `…` |
| Line 2 `w-N-l2` | (12, 54.6, W-284, 28.8), cy 69 | F24 INK: `media_artist`, else the state word (`Idle`, `Off`, `-` loading, `n/a`) |
| Badge | (W-264, 57, 24, 24) | S, failed / unavailable only |
| Prev `w-N-prev-btn` | (W-228, 20, 64, 64), `prev` M at (W-216, 32) | sub-region 1 |
| Play/pause `w-N-play-btn` | (W-152, 20, 64, 64) | sub-region 2: playing/buffering -> **INK-filled, `pause` glyph**; paused/idle/on/standby -> outline, `play` glyph; off -> outline, `power` glyph (`turn_on`) |
| Next `w-N-next-btn` | (W-76, 20, 64, 64), `next` M | sub-region 3 |
| Text block (sub-region 4) | (12, 12, W-264, 80) | opens the popup |

Budget as cover: 61.8 of 80, buttons 80 %. Off: prev/next dotted OFF and inert. Pending: tapped button dashed + one hourglass M
(cover rule), no badge. Unavailable: dotted frame, OFF name, `n/a`, dotted buttons, warn badge. Loading: name + `-`, buttons active.
The player name is shown only when there is no title (idle/off/loading): with several players use their `name` in config.
**Rocker:** INNER mode with stops prev, play/pause (entry), next, text block (PUSH opens the popup). Redraw: lines, the changed buttons, badge.

### 4.1 Media popup

Mockup `widgets-new-popup-media.svg`. Panel (40, 94, 880, 408), same frame, dirty rect and refresh as 1.1.

| Element | Landscape rect / position | Font / grey |
|---|---|---|
| Player name `w-90-name` | x 56, cy 142 | F24 INK |
| Close | (840, 110, 64, 64) | outline |
| Title | x 56, cy 206 (box 182..230), max width 784 | F40 INK, `…` |
| Artist | x 56, cy 254 (box 239.6..268.4) | F24 INK |
| Album | x 56, cy 288 | F18 INK |
| Prev / play-pause / next | (56, 318, 96, 64), (164, 318, 96, 64), (272, 318, 96, 64) | glyph M; play-pause filled while playing |
| State word | x 392, cy 350 | F24 INK `Playing` / `Paused` / `Idle` |
| Rocker hint | right edge 904, cy 350 | F18 DIM static |
| Volume `-` / `+` | (56, 406, 64, 64) / (616, 406, 64, 64) | outline |
| Volume bar | track (132, 426, 472, 24) TRACK grey, fill INK width 472 x level | no border |
| Volume value | x 696, cy 438 | F24 INK `45%` (`Muted` when muted) |
| Mute | (840, 406, 64, 64), `volume` / `mute` M (shows the current state) | outline; muted: filled |

**Interactions.** Touch: transport buttons as in the widget; `-` / `+` step 5 %; mute toggles; close / tap outside / long push
close. **Rocker:** UP / DOWN = volume +5 % / -5 % (no focus ring), PUSH = play/pause, long push = close. `volume_set` is sent
1 s after the last step (like the climate target); the bar and value update at once (pending look: none, the value is the
requested one). Volume controls are dotted/inert when `supported_features` lacks volume. **States:** off -> `Off`, play button
shows `power`; unavailable -> popup does not open; loading -> `-` title. **Refresh:** a volume step redraws the bar + value
(`epd_quality`: the track is grey, one pass); transport buttons `epd_fastest`. Title changes while open redraw the title block
(12..) only.

**Portrait** (not mocked), panel (16, 188, 508, 560), panel-relative: name x 16 cy 48; close (428, 16, 64, 64); title x 16 cy 120
(F40, max width 476); artist cy 168; album cy 202; transport (16, 232, 96, 64), (124, 232), (232, 232), state word x 344 cy 264;
volume row y 320: `-` (16, 320), bar track (92, 340, 324, 24), `+` (428, 320); value x 16 cy 432, mute (428, 400, 64, 64); hint x 16 cy 524.

## 5. Alarm panel, 1x1

**HA:** `alarm_control_panel.*`; `state` (`disarmed`, `armed_home`, `armed_away`, `armed_night`, `armed_vacation`,
`armed_custom_bypass`, `arming`, `pending`, `triggered`), attributes `code_format` (`number` / `text` / none),
`code_arm_required`, `supported_features`. Services `alarm_disarm`, `alarm_arm_home`, `alarm_arm_away`, `alarm_arm_night` with `code`.

Toggle layout (widgets.md 3, budget 73.4 of 80): name F24 (12, 15.6, W-24, 28.8) cy 30; icon M (12, 49, 40, 40); state F24 x 58,
cy 69, max width W-70 (state box landscape 106).

| State | Text | Icon | Look |
|---|---|---|---|
| disarmed | `Off` | `shield_off` | outline |
| armed_home / away / night | `Home` / `Away` / `Night` | `shield` | outline |
| armed_vacation / custom_bypass | `Vacat.` / `Custom` | `shield` | outline (no buttons for them in the popup, v1) |
| arming / pending | `Arming` / `Entry` | `shield_off` | outline |
| triggered | `ALARM` | `shield_alert` | **INK fill**, PAPER content |
| unavailable | `n/a` | `warn` M OFF | dotted OFF frame, OFF name |
| loading | `-` | `shield_off` | outline |

Pending after a command (button in the popup) is shown on the widget as the toggle pending look (dashed 3 px, hourglass badge
(W-36, 57), requested text). **Tap / PUSH opens the keypad popup.** Redraw: whole widget.

### 5.1 Keypad popup

Mockup `widgets-new-popup-alarm.svg`. Panel (40, 94, 880, 408), dirty rect and refresh as 1.1.

| Element | Landscape rect / position | Font / grey |
|---|---|---|
| Name | x 56, cy 142 | F24 INK |
| Close | (840, 110, 64, 64) | outline |
| State | x 56, cy 222, baseline 236 | F40 INK: `Disarmed`, `Armed home`, `Armed away`, `Armed night`, `Arming`, `Pending`, `TRIGGERED` |
| Code box | (56, 262, 300, 64), border 2 | outline; digits as `●` F40 INK at x 72, cy 294; empty: `Code` F24 DIM |
| Message | x 56, cy 358 | F18 INK: `Enter code`, `Code entered, pick a mode`, `Wrong code`, `Failed`, `Sent` |
| Rocker hint | x 56, cy 424 and 450 | F18 DIM static |
| Keys (12) | x = 380, 488, 596, y = 186, 262, 338, 414, 96 x 64 | digits F40; row 4: `backspace` glyph 32, `0`, `C` (clear) |
| Mode buttons (4) | (728, 186 + 76 i, 176, 64): `Disarm`, `Home`, `Away`, `Night` | F24; the current mode is INK-filled |

Rules: code is digits only (max 8), shown as dots, **held in RAM only**, cleared on send, close, sleep and after a wrong-code reply;
`code_format: text` is not supported in v1 (keys dotted, message `Text code not supported`; arming without a code still works if
`code_arm_required` is false). Modes whose feature is missing are dotted OFF and inert. Disarm always needs the code when the panel
has a `code_format`; arm modes need it only when `code_arm_required`. Tapping a mode sends the service; that button shows dashed
border + hourglass M until the next fetch of the entity (re-fetched 2 s after the call); a changed state updates the state line and the
filled button; a service error or unchanged state after 2 fetches shows `Wrong code` / `Failed` and re-enables. The popup stays open until closed.
**Rocker:** a 3 px focus ring walks the 12 keys in reading order, then the 4 mode buttons, then close (UP = previous, DOWN = next,
wraps); entry focus = key `5`; PUSH presses the focused control; long push closes. **Touch redraw:** a key press redraws only the
code box (`epd_fastest`, black/white only); the mode buttons and state line redraw individually. **Portrait** (not mocked), panel
(16, 116, 508, 704), panel-relative: close (428, 16, 64, 64); title x 16 cy 48; state cy 120; code box (16, 152, 476, 64); message cy 242;
keys 150 x 64 at x = 16, 179, 342, y = 264, 340, 416, 492; mode buttons 110 x 64 at y 572, x = 16, 138, 260, 382; hint x 16 cy 668.

## 6. Lock and garage door, 1x1

**HA:** `lock.*` (`locked`, `unlocked`, `locking`, `unlocking`, `jammed`, `open`) with `lock.lock` / `lock.unlock`; garage = `cover.*`
with `device_class: garage` (or config `type: garage` on any cover) with `cover.open_cover` / `close_cover` / `stop_cover`. Locks that
need a code (`code_format`) are not supported in v1 (the widget shows the alert badge and does nothing). Config key
`confirm: true | false` overrides the default below.

Toggle layout as 5 (name F24 cy 30, icon M, state F24 cy 69, budget 73.4):

| Widget | State | Text | Icon | Look |
|---|---|---|---|---|
| lock | locked | `Locked` | `lock` | outline |
| lock | unlocked / open | `Open` | `lock_open` | **INK fill** (as binary_sensor lock: on = open) |
| lock | locking / unlocking | `Locking` / `Opening` | as target | outline, no pending |
| lock | jammed | `Jammed` | `alert` M | INK fill |
| garage | closed | `Closed` | `garage_off` | outline |
| garage | open | `Open` | `garage_on` | INK fill |
| garage | opening / closing | `Opening` / `Closing` | `garage_on` / `garage_off` | outline |
| both | unavailable / loading | `n/a` / `-` | `warn` M OFF / as normal | dotted OFF frame / outline |

**Action.** Tap / PUSH acts on the opposite of the current state: locked -> unlock, unlocked -> lock; garage closed -> open, open ->
close, opening/closing -> `stop_cover` (no dialog); locking/unlocking/jammed: ignored. **Confirm default:** lock: **unlock only**
(locking is safe); garage: **both directions** (a remote door must not move by a stray tap). The confirm dialog (section 7) opens first;
Yes sends the service, and the widget shows the toggle pending look (requested state, dashed border, hourglass badge; on a filled
widget the dashed border is PAPER, see 0). Failed -> alert badge until the next successful fetch. Redraw: whole widget.

## 7. Confirm dialog (shared)

Used by lock/garage, the notifications `Clear all` and any later destructive action. Rocker rule STYLE 10b: **UP = yes, DOWN = no,
PUSH = skip** (never push for yes). Mockup `widgets-new-popup-confirm.svg`. Modal, same border / shadow rules as the popups
(brightness.md 2); a tap outside = skip.

| Element | Landscape | Portrait (not mocked) |
|---|---|---|
| Panel | (200, 188, 560, 220), border 3, shadow +6 | (30, 356, 480, 224) |
| Title | centred at x = panel centre, cy panel +44, F24 INK, `…` to panel width - 40 | same |
| Sub line | centred, cy panel +78, F18 INK: `Skips by itself after 15 s` | same |
| Buttons (3) | y panel +128, 160 x 64, x = 228, 400, 572 (gap 12) | 136 x 64, panel x +24, +172, +320 |
| Button content | label F24 cy button +19, key hint F18 cy button +48: `Yes`/`UP`, `No`/`DOWN`, `Skip`/`PUSH` | same |

Titles: `Unlock Front door?`, `Open Garage?`, `Close Garage?`, `Dismiss all 4?` (name from config / `friendly_name`, truncated).
All three buttons are plain outline (no pre-selected answer). Auto-skip after 15 s, or immediately before the device sleeps.
The widget's focus ring stays on the widget underneath. Refresh: open/close = one `epd_fastest` partial of the panel + shadow
inflated 2 px (the area beneath holds greys only if a GC16 just ran; then `epd_quality` once, STYLE 10). Skip changes nothing.

## 8. Energy, 2x1

**HA:** two numeric `sensor.*` entities that already hold **today's** totals (e.g. a daily `utility_meter`, `unit_of_measurement` `kWh`
or `Wh`; Wh is shown as kWh): config `usage:` and `solar:` (either may be missing; then only that row is drawn centred,
see below). Sizes: 2x1 (364 / 508), no 1x1.

Two bar rows with a shared scale `max = max(used, solar)` so the bars are comparable; fill width = bar width x value / max (at least 2 px
when the value > 0). Reuses the history bar look (history.md 3.3): bar fill and a lighter empty track; deviation: the fill is **INK**
(not the MID "today so far" grey) so the update stays on the fast black/white path; the empty part is the TRACK grey allowed for progress
bars (STYLE 1). The history bar popup keeps MID for the current day.

| Element | Box | Font / grey |
|---|---|---|
| Label `w-N-lab0` / `lab1` | x 12, cy 32 / 72 | F18 INK `Used` / `Solar` |
| Track `w-N-track0` / `track1` | (76, cy - 10, W-188, 20), bw = 176 landscape / 320 portrait | TRACK |
| Bar fill | (76, cy - 10, bw x v / max, 20) | INK |
| Value `w-N-val0` / `val1` | right edge W-12, cy 32 / 72 | F18 INK `7.4 kWh` (`11.2 kWh`; >= 100: no decimal) |

Budget: boxes 21.2..42.8 and 61.2..82.8, bars 22..42 and 62..82, 61.6 of 80 (77 %), horizontal 100 %. Widest value `-12.3 kWh` about 96 px,
ends 4 px after the bar. Alternative: one entity missing -> that row `-` with an empty track; unavailable (both) -> dotted frame, `n/a`
F24 (cy 52), warn badge (W-36, 12); loading -> empty tracks, values `-`. A single unavailable entity only blanks its own row (value `-`).
**Interaction:** read-only; long press (>= 600 ms) / rocker long push opens the history bar popup of `usage` (history.md, type bar, period
hour, today). Redraw: value texts and the bars that changed, each its own rect.

## 9. Grid placement and sizes

| Widget | Portrait | Landscape | Focusable |
|---|---|---|---|
| notifications | 1x1 | 1x1 | yes (opens popup) |
| weather (forecast) | 2x1 | 3x1 | no |
| calendar | 2x1 | 2x1 (3x1 allowed) | no |
| media player | 2x1 | 3x1 | yes (INNER: prev, play/pause, next, text) |
| alarm panel | 1x1 | 1x1 | yes (opens popup) |
| lock / garage | 1x1 | 1x1 | yes (opens dialog) |
| energy | 2x1 | 2x1 | no |

Sub-region rects for the engine (widget-relative): media `(W-228, 20, 64, 64)`, `(W-152, 20, 64, 64)`, `(W-76, 20, 64, 64)`, text `(12, 12, W-264, 80)`;
others: whole widget.

## 10. Refresh summary

- Widget value changes: smallest element rect, `epd_fastest` (all dynamic text is INK; the energy track is the only grey and is static,
  so update only the fill rect).
- Popups open/close: one `epd_quality` partial of the dirty rect; in-popup taps: the control rect only, `epd_fastest`, except the media
  volume bar (grey track, `epd_quality`). GC16 full refresh on tab change and ghost debt unchanged (STYLE 10); popups count their
  feedback refreshes toward the 12-refresh ghost debt, so a long keypad session ends with one full refresh after close.
- Popups close before sleep (brightness.md rule 5); the code buffer is cleared then.

## Open questions

1. Lock/garage confirm defaults: lock = unlock only, garage = open and close (recommended default). 2. Energy popup: usage only
(recommended; a two-series popup later). 3. Media widget hides the player name while a title is shown (recommended). 4. Alarm v1: numeric
codes, modes Home/Away/Night only (recommended). 5. "Filled = needs attention" rule above. 6. Portrait popups are specified but not
mocked up (recommended: mock them once the landscape ones are accepted).

## Evidence

`tools/render_mockups.sh` + `tools/check_mockups.py docs/ui/widgets-new-*.svg`: 0 problems (8 files). PNGs:
`docs/ui/png/widgets-new-{tab-landscape,tab-portrait,states-small,states-wide,popup-notifications,popup-alarm,popup-media,popup-confirm}.png`.
The `w-90` panels, key and row frames use `-btn` ids or no ids; popup text was checked against the padded panel frame.

