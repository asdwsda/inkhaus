# Settings screen

System screen (not config-driven). Opened by a **long push of the rocker at TAB
level** (layout.md 7.2) or by a **tap on the battery cluster** in the status bar
(hit kind `settings`, layout.md section 6). Shown in the current orientation; no
tab bar, no status bar. Values from `docs/ui/STYLE.md` (revision 3: touch targets
64, gaps 12, every text and icon placed by its centre line `cy`). Mockups:
`settings-portrait.svg`, `settings-landscape.svg`. Rect = (x, y, w, h); screen
coordinates unless it says "row-relative".

## 1. Purpose and data sources

Let the user change the five device settings, trigger a refresh, and see
read-only diagnostics.

| Item | Source |
|------|--------|
| Orientation, wake mode, refresh interval, idle timeout, demo mode | `settings::Settings` (lib/settings). Allowed values and cycle order = `kOrientationValues`, `kWakeModeValues`, `kRefreshIntervalMinValues` = {0, 15, 30, 60, 120, 240}, `kIdleTimeoutSValues` = {10, 20, 30, 60}, `demo_mode` (bool, `Off` -> `On`; task 034) |
| Refresh now | the same state fetch as a tap on the "updated" label (ha_client) |
| Firmware version | build-time constant (firmware-dev: needs a `FW_VERSION` define; architect to confirm where it comes from) |
| Battery % and voltage | M5Unified power API, read when the screen opens and after every successful "Refresh now" |
| HA connection | ha_client state: which URL worked last (local / remote), last success time (RTC), or offline |
| Wi-Fi SSID | `connection:` section of config (the configured SSID); suffix `(not connected)` if the last session could not join |
| Demo mode state | the saved `demo_mode` setting (when On, the app runs the embedded demo.yaml and starts no Wi-Fi/HA session; task 033/034) |
| Config status, error count | config module (task 003 validation errors): first error's path and line, total count. Architect to confirm the config model exposes path + line of each error. |

No Home Assistant entity or service is used except the shared state fetch.

## 2. Behaviour rules

1. **Edit buffer.** Opening Settings copies the saved settings into an edit
   buffer. Taps/pushes change only the buffer and the screen.
2. **Saved on leaving, not on every tap.** Each NVS write costs flash wear and
   power. Leaving = Back button, rocker long push, or the idle auto-leave
   (rule 5). On leave: if buffer != saved, `settings::save()` runs once (it
   writes only changed keys). If nothing changed, nothing is written. There is
   no discard button; cycling a row back to its original value removes the
   "unsaved" chip and makes the leave write-free.
3. **When changes take effect.**
   - Orientation: applied on leaving: rotation is set, the layout recomputed
     (active tab kept, sub-page clamped), then **one full refresh** of the tab
     screen (STYLE.md section 10; Settings exit is always a full refresh).
   - Wake mode, refresh interval: used the next time the device prepares to
     sleep (power module reads the saved settings). The sleep indicator variant
     (STYLE.md 5.5) follows the saved wake mode.
   - Idle timeout: used from the moment Settings is left.
   - Demo mode: applied on leaving, like orientation: the config source is
     reloaded (embedded demo.yaml when On, the SD config when Off) and the tab
     screen is drawn with the usual one full refresh. While it is On, no Wi-Fi
     or HA session is started (and Refresh now does nothing but show its failed
     look, see section 6).
4. **Pending changes marker.** When buffer != saved, an INK chip `unsaved` is
   shown in the header. It disappears when equal again.
5. **Idle in Settings.** The idle timer is fixed at 60 s while Settings is open
   (not the edited idle value, so lowering it to 10 s cannot cut the user off
   mid-edit). On expiry the screen auto-leaves (saves, applies orientation,
   draws the tab screen with a full refresh) and then sleeps, so the frozen
   image is never a stale Settings page; the sleep indicator is then drawn on
   the tab screen. OPEN-S1.
6. **Touch Wake note.** While wake mode is Touch or Touch & Button (in the
   buffer), the note slot shows an alert box `Touch wake shortens battery life. / Button wake is
   recommended.` No numbers are quoted (power is still an estimate in
   ARCHITECTURE.md). The wake row always shows its pips (no warn icon; task 039).
   With Button the note slot is empty.
7. **Value cycling.** Tap or push on a row moves to the next value in the
   `settings.h` array and wraps from the last to the first. There is no
   "previous" (longest cycle is 6 values; Demo mode cycles Off -> On -> Off).

## 3. Layout, portrait (540 x 960)

Margins 16, gaps between targets 12. White background. **All text that can change (setting values, hints, info values, chip, button labels) is INK** (STYLE.md section 1, revision 5); the only grey text is the static info labels (F18 DIM) and the OFF hairlines.

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Title | x = 16, box (16, 0, 200, 54) | 27 | F40 INK, left | `Settings` |
| Unsaved chip | (404, 11, 120, 32) | 27 | F18 PAPER on INK fill, radius 8, centred | `unsaved` (only when dirty) |
| Header rule | (0, 54, 540, 2) | | INK fill | |
| Row 1 Orientation | (16, 68, 508, 68) | 102 | border 2 INK, radius 8, PAPER | button |
| Row 2 Wake mode | (16, 148, 508, 68) | 182 | same | button |
| Row 3 Refresh every | (16, 228, 508, 68) | 262 | same | button |
| Row 4 Idle timeout | (16, 308, 508, 68) | 342 | same | button |
| Row 5 Demo mode | (16, 388, 508, 68) | 422 | same | button |
| Note slot | (16, 468, 508, 64) | 500 | see 3.2 | touch-wake warning |
| Info hairline + rows | (16, 548 ... 852) | | see 3.3 | read-only |
| Refresh now button | (16, 880, 248, 64) | 912 | border 2, radius 8 | action |
| Back button | (276, 880, 248, 64) | 912 | border 2, radius 8 | action |

Row height is 68 (was 88 with four rows): 5 x 68 + 4 x 12 = 388, the same block height as before, so nothing below the rows moves. Vertical sums: header 56; +12 = 68; rows 388 -> 456; +12 = 468;
note slot 64 -> 532; +16 = 548; info 5 x 48 + 64 = 304 -> 852; +28 = 880; footer 64 ->
944; +16 = 960. Horizontal: 16 + 248 + 12 + 248 + 16 = 540 (the footer buttons
use the widget grid's columns).

### 3.1 Setting row (row-relative, width 508, height 68)

Centre line of the row: cy = 34. Label and hint form a two-line block centred on 34
(label F24 cy = 24 -> 12..36, hint cy = 47 -> 38..56, i.e. exactly inside the 12 px
widget padding the checker uses); the value is centred on cy = 34.

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Label | x = 16, box (16, 12, 220, 24) | 24 | F24 INK, left | `Orientation`, `Wake mode`, `Refresh every`, `Idle timeout`, `Demo mode` |
| Hint / pips | (16, 38, 220, 18) | 47 | see below | |
| Value | right edge x = 492, box (240, 0, 252, 68) | 34 | F40 INK, right | value text |

Pips (hint line): one circle of diameter 12 per allowed value, centres at
row-relative x = 22 + 20*k, cy = 47; the current value's pip is INK-filled,
the others are 2 px outlines (shape, not grey). 2 pips for orientation and
demo mode, 3 for wake mode, 6 for refresh, 4 for idle. The warn icon next to the
wake value is removed (task 039).

Value texts: Orientation `Portrait` / `Landscape`; Wake `Button` / `Touch` / `Touch & Button`;
Refresh `Off`, `15 min`, `30 min`, `60 min`, `120 min`, `240 min`; Idle `10 s`,
`20 s`, `30 s`, `60 s`; Demo mode `Off` / `On`. Widest: `Landscape` at F40 is about 215 px, fits the
252 px value rect and leaves room next to the longest label (`Refresh every`, about
163 px at F24: 16 + 163 = 179 < 508 - 16 - 215 = 277).

### 3.2 Note slot (508 x 64)

Empty (nothing drawn, no border) unless wake mode is Touch or Touch & Button. Then: 2 px INK border,
radius 8; alert icon (M) at slot-relative (12, 12, 40, 40), cy = 32; text lines
(F18 INK, left) from x = 64 with cy = 21 and cy = 43 (block centred on 32):
`Touch wake shortens battery life.` / `Button wake is recommended.`

### 3.3 Info rows

Each row has a 1 px OFF hairline on its top edge (16, y, 508, 1). Label at
x = 0 (row-relative), F18 DIM (static caption), left, row cy = h/2; value at x = 158, F24 INK, left,
same cy (config row: two lines at h/2 -14 and h/2 +14). When a status icon (S,
24x24) is needed it sits at (158, cy-12) and the value text moves to x = 190. Text
fitting rule: F24, then F18, then `…`.

| Row | Rect | cy | Label | Value |
|-----|------|----|-------|-------|
| Firmware | (16, 548, 508, 48) | 572 | `Firmware` | `v0.3.1` |
| Battery | (16, 596, 508, 48) | 620 | `Battery` | `78%, 3.92 V` (alert icon if <= 15 %, bolt icon if charging) |
| HA | (16, 644, 508, 48) | 668 | `Home Assistant` | `Demo mode` (demo mode On) / `local, ok 14:05` / `remote, ok 14:05` / alert icon + `offline, last ok 13:05` / alert icon + `offline, never ok` / `-` before the first session |
| Wi-Fi | (16, 692, 508, 48) | 716 | `Wi-Fi` | `Demo mode` (demo mode On), or SSID, or `SSID (not connected)`, or `-` |
| Config | (16, 740, 508, 64) | 772 (lines 758 / 786) | `Config` | `OK`; or alert icon (cy 758) + first error up to 2 lines: `tabs[0].widgets[2].entity (line 34)`; or warn icon + `no config.yaml on SD`; `built-in demo` while demo mode is On (the SD config is not read; OPEN-S4) |
| Errors | (16, 804, 508, 48) | 828 | `Config errors` | count (`0`, `3`), `-` when the config is missing; `0` in demo mode |

Times: same day `14:05`; earlier day `26.10. 13:05`.

### 3.4 Footer buttons (button-relative, 248 x 64)

| Element | Rect | cy | Font |
|---------|------|----|------|
| Icon (M) | (12, 12, 40, 40) | 32 | `refresh` / `chevron_left`, INK; while a refresh runs the hourglass (M), after a failed one the alert icon (M), in place of the refresh glyph |
| Label | x = 60, box (60, 0, W-68, 64), left | 32 | F24: `Refresh now` / `Back` |

(There is no separate badge: the status shows in the icon slot, so the label
never collides with it. `Refresh now` is about 149 px at F24, ends at x = 209.)

## 4. Layout, landscape (960 x 540)

Two columns: settings on the left (16..472), info and actions on the right
(488..944), 16 px between columns.

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Title | x = 16, box (16, 0, 200, 50) | 25 | F40 INK, left | `Settings` |
| Unsaved chip | (824, 9, 120, 32) | 25 | F18 PAPER on INK, radius 8, centred | `unsaved` (when dirty) |
| Header rule | (0, 50, 960, 2) | | INK | |
| Row 1 Orientation | (16, 64, 456, 68) | 98 | as portrait | |
| Row 2 Wake mode | (16, 144, 456, 68) | 178 | | |
| Row 3 Refresh every | (16, 224, 456, 68) | 258 | | |
| Row 4 Idle timeout | (16, 304, 456, 68) | 338 | | |
| Row 5 Demo mode | (16, 384, 456, 68) | 418 | | |
| Note slot | (16, 464, 456, 64) | 496 | see below | |
| Info rows | (488, 64 ... 416, 456, ...) | | as portrait, value x = 158 (width 298) | |
| Refresh now button | (488, 464, 222, 64) | 496 | | |
| Back button | (722, 464, 222, 64) | 496 | | |

Layout choice: all five rows get the portrait height 68 (a 64 px target plus 4 px
for the border; 5 x 68 + 4 x 12 = 388, identical to the old 4 x 88 + 3 x 12). The
rows stay a single column, the note slot and footer keep their y, the right
column is untouched, and portrait and landscape share one row design. A 2-column
row grid would have halved the row width to 222 and squeezed `Landscape` (F40,
215 px) against its label; moving an info row would not free any left-column height.
Vertical sums (left column): header 52; +12 = 64; rows 388 ->
452; +12 = 464; note slot 64 -> 528; +12 = 540. Right column: info 64 -> 416,
gap 48, footer 464 -> 528, +12 = 540. Horizontal: 16 + 456 + 16 + 456 + 16 = 960.
Footer: 488 + 222 + 12 + 222 = 944.

Landscape row internals (row-relative, width 456, height 68): identical to
portrait with width 456: label x = 16, cy = 24; pips x = 22 + 20*k, cy = 47; value
right edge x = 440, cy = 34; Note slot (456 x 64): border 2, radius 8; alert icon (M) at
(12, 12, 40, 40); text from x = 64, cy = 21 and 43 (`Touch wake shortens battery
life.` is about 313 px, fits the 380 px left).

Info rows (unchanged; x = 488, width 456, hairline on each top edge; label x = 0, value
x = 158 row-relative = 646 absolute):

| Row | Rect | cy |
|-----|------|----|
| Firmware | (488, 64, 456, 56) | 92 |
| Battery | (488, 120, 456, 56) | 148 |
| HA | (488, 176, 456, 56) | 204 |
| Wi-Fi | (488, 232, 456, 56) | 260 |
| Config | (488, 288, 456, 72) | 324 (lines 310 / 338) |
| Errors | (488, 360, 456, 56) | 388 |

Heights: 4 x 56 + 72 + 56 = 352; 64 + 352 = 416. With demo mode On the HA and Wi-Fi rows read `Demo mode`, Config `built-in demo`, Errors `0` (landscape mockup). The config value has 298 px (266
with an icon); `tabs[0].widgets[2].entity` is about 298 px at F24 and 224 px at F18,
so here it falls to F18 or wraps (fit rule).

## 5. Interactions

### Touch

| Target | Action | Redraw (partial) |
|--------|--------|------------------|
| Setting row (whole rect) | cycle to the next value in the edit buffer | row value rect + hint rect; note slot if wake mode changed; unsaved chip if dirty flag flipped |
| Refresh now | start the state fetch (ignored while one runs) | button (pending look: dashed 3 px border, hourglass M instead of the icon); on completion: button, HA row, Battery row |
| Back | leave (rule 2/3) | full refresh |
| anything else | nothing | |

All targets are >= 64 x 64 (rows 68 high, footer 64 high) with >= 12 px between
them (rows are 12 apart, footer buttons 12 apart; landscape columns 16 apart).
Settings is **entered by touch** with a tap on the battery cluster of the tab
screen (hit kind `settings`); there is no status bar inside Settings.

### Rocker

Focus stops in order: 1 Orientation, 2 Wake mode, 3 Refresh every, 4 Idle
timeout, 5 Demo mode, 6 Refresh now, 7 Back. Entry focus = stop 1. The ring is the standard
focus ring (3 px, 3..6 px outside the rect; the 12 px gaps and 16 px margins
hold it; ring dirty region = rect inflated by 6).

| Key | Action |
|-----|--------|
| PREV (up) | previous stop, wraps 1 -> 7 |
| NEXT (down) | next stop, wraps 7 -> 1 |
| PUSH | on a row (stops 1-5): cycle value; on Refresh now: start fetch; on Back: leave (this is not a dialog; for any yes/no dialog: UP = yes, DOWN = no, PUSH = skip, STYLE.md section 10b) |
| LONG_PUSH | leave (same as Back), from any stop |

Ring move redraws old and new rects (inflated by 6), partial. Key mapping follows
layout.md 7.1 (`PREV`/`NEXT`, invert flag).

### Refreshes

- Enter and leave Settings: GC16 full refresh (STYLE.md section 10).
- Everything else above: one partial refresh of the listed region, no clean-up
  pass. Value texts are INK, so a row value cycles with `epd_fastest`; a region
  that still holds greys (the hint line is black/white, but info rows hold DIM
  labels and OFF hairlines) gets one `epd_quality` refresh instead. Each step
  counts towards ghost debt (12 feedback refreshes trigger a GC16 full refresh at
  the next quiet moment, at the latest before sleep).

## 6. States

| State | Look |
|-------|------|
| Normal | as the mockups |
| HA offline / unreachable | HA row: alert icon + `offline, last ok 13:05` (or `offline, never ok`). Settings and Back work. Refresh now stays enabled: it tries, and on failure shows the alert icon (M) in place of its glyph until the next success. |
| Refresh running | Refresh now: dashed 3 px border, hourglass (M) instead of its icon. HA row shows hourglass (S) + `refreshing…`. Rows stay active. |
| Demo mode On | HA row and Wi-Fi row: `Demo mode`; Config row `built-in demo`, Errors `0`. Refresh now is shown but starts no fetch (it takes its failed look, alert icon, for one tick). Battery and Firmware as usual. The `unsaved` chip shows until Settings is left, then the config source is reloaded. |
| Demo mode Off | HA / Wi-Fi / Config rows as in the other states above. |
| Wi-Fi not joined | Wi-Fi row: `SSID (not connected)` |
| Config missing | Config row: warn icon + `no config.yaml on SD`. Errors row `-`. HA and Wi-Fi rows show `-` if the connection section is gone with the file. Settings remain fully usable (this is how a user gets a device back to a sane state). |
| Config with errors | Config row: alert icon + the **first** error as `path (line N)` (up to 2 lines, then `…`); Errors row shows the total, e.g. `3`. |
| Low battery (<= 15 %) | Battery row has the alert icon; nothing blocks. |
| Charging | Battery row has the bolt icon. |
| Stale info | Values are a snapshot from opening the screen (battery, HA time). They update after a successful Refresh now only. The HA row's time is the staleness indicator. |

Loading state: none (everything is read synchronously when the screen opens).

## 7. OPEN items

- OPEN-S1: idle timeout inside Settings fixed at 60 s (proposal).
- (resolved) OPEN-S2: the touch path to Settings is a tap on the battery cluster.
- OPEN-S4: in demo mode the Config row text `built-in demo` and the inert Refresh now are proposals (task 034 does not say); architect to confirm.
- OPEN-S3: first line of info only shows the first config error; a scrollable
  list is not specified.
- Questions for the architect: where `FW_VERSION` comes from; whether the config
  module exposes path + line + count for errors.
