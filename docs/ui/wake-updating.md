# Wake: "not fresh yet" look (task 127, experiment, user picks A or B)

Status: proposal. Mockups (PNG in `experiments/png/`): `docs/ui/experiments/wake-updating-portrait.svg`,
`docs/ui/experiments/wake-updating-landscape.svg` (rows: 1 updating, 2 tap during updating, 3 end state offline; column A | B;
portrait also has the status-bar sequence strips). Follows STYLE.md 3, 5.3, 9, 10 and `ready-indicator.md` (task 120).
Entities: none new; input is the net/fetch state (task 116) and the pending queue (task 126).

## Purpose
After an interactive wake the frozen frame looks live. Until the fresh states arrive every widget that shows an HA state gets a cheap
"not fresh yet" look (`stale`), the status bar says the fetch runs, and when the states are in, everything is redrawn in ONE refresh.
Applies to toggle, cover, climate, vacuum, sensor, binary_sensor, weather. Not applied to: unavailable widgets (dotted frame stays,
taps ignored), history widgets (own look), tab bar, status bar. Not drawn on scheduled (RTC) wakes. Stale look is a global flag: a tab
change while updating draws the new tab already stale (it is a GC16 full refresh anyway).

## Must differ from pending (STYLE 9)
| | Pending (per tap) | Stale (wake) |
|---|---|---|
| Frame | 3 px dashed 8 on / 6 off, whole frame | A: four corner brackets, no sides. B: 2 px solid OFF |
| Badge (S, slot per widgets.md) | hourglass | clock (same icon as "outdated" in the status bar) |
| Contrast | full INK | A: full INK; B: all OFF grey, on-fill = OFF grey |
| Scope | one widget / button | all widgets of the tab |
| Touch | active | active (taps go out if ready, else queue) |
Reading rule: hourglass = my command is on its way; clock = this value is old.

## Variant A: pure black/white (recommended)
Per widget (W x 104, widget-relative): the 2 px frame is removed and replaced by corner brackets: 3 px INK, arm 20 px, outer radius 8,
flush with the widget edge (path `M1.5 20 V9.5 A8 8 0 0 1 9.5 1.5 H20`, mirrored for the 4 corners). Filled (on) widgets keep the INK
fill; brackets are PAPER, inset 3 px. Content (names, values, icons, buttons) unchanged and INK. Clock badge S (24 x 24) in the
widget's badge slot (toggle (W-36, 57); sensor/weather: (W-36, 57) at the caption line, because the F40 value would hit (W-36, 12) in
a 176 px cell; climate (W-312, 24); cover (W-264, 57)). Greys: none new.
Refresh: `epd_fastest` (~130 ms, only B/W changes: frame ring + badge). Sensor/weather DIM captions are not touched (same pixels).

## Variant B: greyed
Per widget: frame 2 px OFF (#888) solid, names/values/icons/captions/button outlines/glyphs OFF; on-fill becomes OFF grey with PAPER
content; clock badge in OFF. Shape (fill vs outline) still says on/off. Breaks "dynamic text is pure black" on purpose; the grey is
the signal, so it is not usable as the only carrier of other states on the same screen (unavailable stays dotted).
Refresh: the area holds greys, so `epd_quality` over the whole content area (one slower, visibly flashing update, ~0.5 s estimate,
verify on device). Any tap during updating (pending look over grey) is also an `epd_quality` partial, the end refresh is quality too.

## Refresh cost (the "one partial + one refresh" budget)
| Step | A | B |
|---|---|---|
| Wake: stale look + status `not ready, connecting` | 1 x fastest, rect = union of changed rects (portrait (0, 0, 540, 868), landscape (0, 0, 960, 524); only changed pixels flip) | 1 x quality, same rect |
| Connecting -> Ready/`updating` (status only, task 120 rule, coalesced < 400 ms) | 0 or 1 small fastest, status region | same |
| Tap during updating | 1 x fastest, widget rect (+ status region on first queued tap) | 1 x quality per tap |
| Fresh states in | 1 refresh: stale look removed + changed values + status `updated`; quality if a changed region holds greys (sensor captions), GC16 if more than 6 VALUES changed (STYLE 10); removing the stale look does not count as a changed widget | same |
| Ghost debt | wake partial adds its area (~0.9 screen) to the fastest-area counter; quality/GC16 adds none | none |
Total interactive wake: A = 1 fastest + (status partial) + 1 end refresh; B = 2 quality + status partial. The only extra over today is
the one wake partial. A is cheaper (about 130 ms vs ~0.5 s) and keeps taps cheap; B is calmer to look at but flashes twice.

## Recommendation: A
Same one-partial budget, fastest mode, no grey, taps during updating stay cheap, and it never conflicts with the INK-text rule. B's
only edge is that grey reads "dim = not live" more instantly; A's brackets + clock do read as "unfinished frame" in the mockups, but
the user should judge on the device.

## Status-bar "refresh running" (existing rects, F18 INK, no new region)
| Phase | Icon (S) | Portrait | Landscape (2 lines) | Clock |
|---|---|---|---|---|
| Wake, not ready | `wifi-join` | `not ready, connecting` | `not ready` / `connecting` | no |
| Ready, fetch running (NEW) | `refresh` (open circular arrow, already in the icon set) | `updating, taps ok` | `updating` / `taps ok` | no |
| Tap queued, not ready | `hourglass` | `queued, connecting` | `queued` / `connecting` | no |
| Fresh | `wifi` | `updated 14:41` | `updated` / `14:41` | per 5.3 |
| Timeout | `wifi-off` | `offline, last 14:05` | `offline` / `14:05` | yes |
| Timeout, tap queued | `hourglass` | `offline, queued` | `queued` / `offline` | yes |
Replaces task 120's silent `ready` while the fetch runs (`updating` is ready AND fetching; `ready` alone is no longer shown after a
wake). Manual refresh tap keeps `fetching states`.

## Tapped widget during updating
Pending look of STYLE 9 on top of the stale look: the widget leaves the stale look (brackets/grey gone), is drawn at full INK with the
requested state, 3 px dashed frame, hourglass badge instead of clock (climate/cover: dashed tapped button, climate target shows the
requested value). Neighbours stay stale. Command goes out at once if ready, else queued (task 120). When the fresh states arrive the
widget settles through the normal confirmed/failed path inside the same end refresh. Rocker: focus ring and actions as today; a rocker
action counts as a tap.

## End states
- Fresh: row 3 of the mockups without the offline status = normal screen. One refresh as costed above.
- Timeout (net timeout of task 116, no new value): ends the updating look with ONE refresh (A fastest, B quality) of the rect of
  the stale widgets + status: widgets return to the normal look with their last values (STYLE 5.3: values are never blanked), status
  `offline, last 14:05` + clock. Queued taps stay pending (dashed + hourglass), status `offline, queued`; existing Failed rules apply
  when they are dropped. Unavailable widgets unchanged throughout.
- Loading (no cache): widgets show `-`, stale look applies the same.

## Interactions summary
| Input | Result | Redraw |
|---|---|---|
| Wake (touch/rocker/side button), waking tap consumed | stale look + `not ready, connecting` | 1 partial (union rect) |
| Tap/rocker on widget | pending look as above | widget rect (+ status on first queued) |
| Tap on status label | ignored while updating (as 120: ignored while connecting; refresh already running) | none |
| Fetch done / timeout | normal look, status `updated` / `offline` | 1 refresh |
