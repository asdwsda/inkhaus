# Settings screen

System screen (not config-driven). Opened by a **long push of the rocker at TAB
level** (layout.md 7.2) or by a **tap on the battery cluster** in the status bar
(hit kind `settings`, layout.md section 6). Shown in the current orientation; no
tab bar, no status bar. Values from `docs/ui/STYLE.md` (revision 3: touch targets
64, gaps 12, every text and icon placed by its centre line `cy`). Mockups:
`settings-portrait.svg`, `settings-landscape.svg`, `tools-portrait.svg`,
`tools-landscape.svg` (revision 6, task 041: the main page has four setting
rows and a **Tools** row that opens the Tools page, section 8). Rect = (x, y, w, h); screen
coordinates unless it says "row-relative".

## 1. Purpose and data sources

Let the user change the device settings (four on this page, two more on the Tools page, section 8), trigger a refresh, and see
read-only diagnostics.

| Item | Source |
|------|--------|
| Orientation, wake mode, refresh interval, idle timeout (main page); demo mode, debug log (Tools page) | `settings::Settings` (lib/settings). Allowed values and cycle order = `kOrientationValues`, `kWakeModeValues`, `kRefreshIntervalMinValues` = {0, 15, 30, 60, 120, 240}, `kIdleTimeoutSValues` = {10, 20, 30, 60}, `demo_mode` (bool, `Off` -> `On`; task 034), `debug_log` (bool, `Off` -> `On`, NVS `dbglog`, default Off; task 041) |
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
1a. **One edit buffer for both pages.** The Tools page edits the same buffer; the `unsaved` chip shows on both pages; going to Tools and back (Back or long push on Tools) saves nothing and applies nothing.
2. **Saved on leaving, not on every tap.** Each NVS write costs flash wear and
   power. Leaving = Back button on the main page, rocker long push on the main page, or the idle auto-leave
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
   - Debug log: used from the moment Settings is left (SD logging starts/stops; no SD access while Off).
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
   "previous" (longest cycle is 6 values; Demo mode and Debug log cycle Off -> On -> Off).

## 3. Layout, portrait (540 x 960)

Margins 16, gaps between targets 12. White background. **All text that can change (setting values, hints, info values, chip, button labels) is INK** (STYLE.md section 1, revision 5); the only grey text is the static info labels (F18 DIM) and the OFF hairlines.

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Title | x = 16, box (16, 0, 200, 54) | 27 | F40 INK, left | `Settings` |
| Unsaved chip | (404, 11, 120, 32) | 27 | F18 PAPER on INK fill, radius 8, centred | `unsaved` (only when dirty) |
| Header rule | (0, 54, 540, 2) | | INK fill | |
| Row 1 Orientation | (16, 68, 508, 72) | 104 | border 2 INK, radius 8, PAPER | button |
| Row 2 Wake mode | (16, 152, 508, 72) | 188 | same | button |
| Row 3 Refresh every | (16, 236, 508, 72) | 272 | same | button |
| Row 4 Idle timeout | (16, 320, 508, 72) | 356 | same | button |
| Tools row | (16, 404, 508, 64) | 436 | border 2 INK, radius 8, PAPER | navigation button, see 3.5 |
| Note slot | (16, 480, 508, 64) | 512 | see 3.2 | touch-wake warning |
| Info hairline + rows | (16, 560 ... 864) | | see 3.3 | read-only |
| Refresh now button | (16, 880, 248, 64) | 912 | border 2, radius 8 | action |
| Back button | (276, 880, 248, 64) | 912 | border 2, radius 8 | action |

Row height is 72 (the demo-mode row moved to the Tools page, so four rows: 4 x 72 + 3 x 12 = 324). The freed height is spent as: rows 72 (a 64 px target plus room for the two-line label/hint block), a 12 px gap, the 64 px Tools row, a 12 px gap, then the note slot. Vertical sums: header 56; +12 = 68; rows 324 -> 392; +12 = 404; Tools 64 -> 468; +12 = 480; note slot 64 -> 544; +16 = 560; info 5 x 48 + 64 = 304 -> 864; +16 = 880; footer 64 -> 944; +16 = 960. The info rows keep their size; only their y moved (+12). Horizontal: 16 + 248 + 12 + 248 + 16 = 540.

### 3.1 Setting row (row-relative, width 508, height 72)

Centre line of the row: cy = 36. Label and hint form a two-line block centred on 36
(label F24 cy = 26, hint cy = 49, inside the 12 px widget padding the checker uses);
the value is centred on cy = 36. (Landscape rows are 68 high: cy = 34, label 24, hint 47.)

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Label | x = 16, box (16, 12, 220, 28) | 26 | F24 INK, left | `Orientation`, `Wake mode`, `Refresh every`, `Idle timeout` |
| Hint / pips | (16, 40, 220, 18) | 49 | see below | |
| Value | right edge x = 492, box (240, 0, 252, 72) | 36 | F40 INK, right | value text |

Pips (hint line): one circle of diameter 12 per allowed value, centres at
row-relative x = 22 + 20*k, cy = 49 (portrait) / 47 (landscape); the current value's pip is INK-filled,
the others are 2 px outlines (shape, not grey). 2 pips for orientation, 3 for wake mode, 6 for refresh, 4 for idle. The warn icon next to the
wake value is removed (task 039).

Value texts: Orientation `Portrait` / `Landscape`; Wake `Button` / `Touch` / `Touch & Button`;
Refresh `Off`, `15 min`, `30 min`, `60 min`, `120 min`, `240 min`; Idle `10 s`,
`20 s`, `30 s`, `60 s`. Widest: `Landscape` at F40 is about 215 px, fits the
252 px value rect and leaves room next to the longest label (`Refresh every`, about
163 px at F24: 16 + 163 = 179 < 508 - 16 - 215 = 277).

### 3.2 Note slot (508 x 64)

Empty (nothing drawn, no border) unless wake mode is Touch or Touch & Button. Then: 2 px INK border,
radius 8; alert icon (M) at slot-relative (12, 12, 40, 40), cy = 32; text lines
(F18 INK, left) from x = 64 with cy = 21 and cy = 43 (block centred on 32):
`Touch wake shortens battery life.` / `Button wake is recommended.`

### 3.5 Tools row (button-relative, 508 x 64; landscape 456 x 64)

Not a setting: it opens the Tools page. Look: same border as the rows, but no pips and a chevron, so it reads as navigation. Label `Tools` F24 INK at x = 16, cy = 32. Summary F18 INK, right-aligned at x = W - 64 (444 / 392), cy = 32: `Demo Off, Log Off` (current buffer values, so a user sees what is hidden behind it). Chevron_right icon (M, 40 x 40) at (W - 52, 12), cy = 32.

### 3.3 Info rows

Each row has a 1 px OFF hairline on its top edge (16, y, 508, 1). Rows in the table are in portrait; landscape has its own table in section 4. Label at
x = 0 (row-relative), F18 DIM (static caption), left, row cy = h/2; value at x = 158, F24 INK, left,
same cy (config row: two lines at h/2 -14 and h/2 +14). When a status icon (S,
24x24) is needed it sits at (158, cy-12) and the value text moves to x = 190. Text
fitting rule: F24, then F18, then `…`.

| Row | Rect | cy | Label | Value |
|-----|------|----|-------|-------|
| Firmware | (16, 560, 508, 48) | 584 | `Firmware` | `v0.3.1` |
| Battery | (16, 608, 508, 48) | 632 | `Battery` | `78%, 3.92 V` (alert icon if <= 15 %, bolt icon if charging) |
| HA | (16, 656, 508, 48) | 680 | `Home Assistant` | `Demo mode` (demo mode On) / `local, ok 14:05` / `remote, ok 14:05` / alert icon + `offline, last ok 13:05` / alert icon + `offline, never ok` / `-` before the first session |
| Wi-Fi | (16, 704, 508, 48) | 728 | `Wi-Fi` | `Demo mode` (demo mode On), or SSID, or `SSID (not connected)`, or `-` |
| Config | (16, 752, 508, 64) | 784 (lines 770 / 798) | `Config` | `OK`; or alert icon (cy 758) + first error up to 2 lines: `tabs[0].widgets[2].entity (line 34)`; or warn icon + `no config.yaml on SD`; `built-in demo` while demo mode is On (the SD config is not read; OPEN-S4) |
| Errors | (16, 816, 508, 48) | 840 | `Config errors` | count (`0`, `3`), `-` when the config is missing; `0` in demo mode |

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
| Tools row | (16, 384, 456, 64) | 416 | see 3.5 | summary right edge x = 392 |
| Note slot | (16, 460, 456, 64) | 492 | see below | |
| Info rows | (488, 64 ... 416, 456, ...) | | as portrait, value x = 158 (width 298) | |
| Refresh now button | (488, 464, 222, 64) | 496 | | |
| Back button | (722, 464, 222, 64) | 496 | | |

Layout choice: the four rows keep the height 68 (landscape is height-limited; 4 x 68 + 3 x 12 = 308). Vertical sums (left column): header 52; +12 = 64; rows 308 -> 372; +12 = 384; Tools 64 -> 448; +12 = 460; note slot 64 -> 524; +16 = 540. Right column and footer are unchanged (info 64 -> 416, footer 464 -> 528). Horizontal: 16 + 456 + 16 + 456 + 16 = 960. Footer: 488 + 222 + 12 + 222 = 944.

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
| Tools row | open the Tools page (section 8); no save | full refresh (GC16) |
| Back | leave (rule 2/3) | full refresh |
| anything else | nothing | |

All targets are >= 64 x 64 (rows 72 / 68 high, Tools and footer 64 high) with >= 12 px between
them (rows are 12 apart, footer buttons 12 apart; landscape columns 16 apart).
Settings is **entered by touch** with a tap on the battery cluster of the tab
screen (hit kind `settings`); there is no status bar inside Settings.

### Rocker

Focus stops in order: 1 Orientation, 2 Wake mode, 3 Refresh every, 4 Idle
timeout, 5 Tools, 6 Refresh now, 7 Back. Entry focus = stop 1. The ring is the standard
focus ring (3 px, 3..6 px outside the rect; the 12 px gaps and 16 px margins
hold it; ring dirty region = rect inflated by 6).

| Key | Action |
|-----|--------|
| PREV (up) | previous stop, wraps 1 -> 7 |
| NEXT (down) | next stop, wraps 7 -> 1 |
| PUSH | on a row (stops 1-4): cycle value; on Tools: open the Tools page; on Refresh now: start fetch; on Back: leave (this is not a dialog; for any yes/no dialog: UP = yes, DOWN = no, PUSH = skip, STYLE.md section 10b) |
| LONG_PUSH | leave (same as Back), from any stop |

Ring move redraws old and new rects (inflated by 6), partial. Key mapping follows
layout.md 7.1 (`PREV`/`NEXT`, invert flag).

### Refreshes

- Enter and leave Settings, and the transitions main page <-> Tools page: GC16 full refresh (STYLE.md section 10; sub-page change).
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
- OPEN-S5: Tools summary text (`Demo Off, Log Off`) is a proposal.
- (resolved) OPEN-S2: the touch path to Settings is a tap on the battery cluster.
- OPEN-S4: in demo mode the Config row text `built-in demo` and the inert Refresh now are proposals (task 034 does not say); architect to confirm.
- OPEN-S3: first line of info only shows the first config error; a scrollable
  list is not specified.
- Questions for the architect: where `FW_VERSION` comes from; whether the config
  module exposes path + line + count for errors.

## 8. Tools page (task 041)

Second system page, reached from the **Tools** row of the main page. Mockups: `tools-portrait.svg` (demo Off, debug log On, focus on Debug log, note visible, unsaved), `tools-landscape.svg` (demo On, debug log Off, focus on Back). Header as the main page (title `Tools`, `unsaved` chip when the shared buffer is dirty). No tab bar, no status bar.

### 8.1 Layout, portrait (540 x 960)

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Title | (16, 0, 200, 54) | 27 | F40 INK | `Tools` |
| Unsaved chip, header rule | as 3 | | | |
| Row 1 Demo mode | (16, 68, 508, 88) | 112 | border 2, radius 8 | button |
| Row 2 Debug log | (16, 168, 508, 88) | 212 | same | button |
| Note slot | (16, 268, 508, 64) | 300 | as 3.2 | only when Debug log = On |
| (empty) | 332 ... 880 | | | |
| Back button | (276, 880, 248, 64) | 912 | as 3.4 | same place as Back on the main page |

Row internals (row-relative, 508 x 88): label F24 INK x = 16, cy = 32 (`Demo mode`, `Debug log`); hint F18 INK x = 16, cy = 62 (`Sample dashboard, offline`, `Log lines to SD card`); value F40 INK right edge x = 492, cy = 44 (`Off` / `On`). No pips (2-value rows carry a hint instead). Row height 88 (the old four-row height; this page has room). Note slot text: `Writes /debug.log (max 1 MB).` / `Uses more battery.`, alert icon M, lines cy = 21 / 43. Vertical sums: header 56, +12, rows 88 + 12 + 88 = 188 -> 256, +12 = 268, note 64 -> 332.

### 8.2 Layout, landscape (960 x 540)

| Element | Rect | cy |
|---------|------|----|
| Title, chip, rule | as the main page landscape (header 52) | 25 |
| Row 1 Demo mode | (16, 64, 456, 88) | 108 |
| Row 2 Debug log | (488, 64, 456, 88) | 108 |
| Note slot (Debug log On) | (488, 164, 456, 64) | 196 |
| Back button | (722, 464, 222, 64) | 496 |

Two columns of 456 (16 px apart) so the page matches the main page's column grid. Row internals as 8.1 with width 456 (value right edge x = 440).

### 8.3 Interactions

| Target | Action | Redraw |
|--------|--------|--------|
| Demo mode row | toggle `demo_mode` in the buffer (Off <-> On) | row value; unsaved chip if dirty flipped (partial) |
| Debug log row | toggle `debug_log` in the buffer | row value; note slot appears/clears; chip (partial) |
| Back | return to the main page (no save, no apply); main page focus = Tools row | GC16 full |
| anything else | nothing | |

Rocker: focus stops 1 Demo mode, 2 Debug log, 3 Back (portrait top to bottom, landscape left to right then Back). Entry focus = stop 1. PREV/NEXT move and wrap. PUSH on a row toggles, on Back returns. **LONG_PUSH = Back** (one level up, not out of Settings). The Back target and the row targets are >= 64 px (rows 88, Back 64), >= 12 px apart. Focus ring as in section 5 (dirty rect inflated by 6; the rows' 12 px gap and 16 px margins hold it).

Saving and effects: unchanged (rules 1a to 3): everything is saved and applied when Settings is left from the main page (Back, long push there, or the idle auto-leave, which also fires from the Tools page after 60 s and leaves Settings entirely, saving the buffer).

### 8.4 States

| State | Look |
|-------|------|
| Normal | as the mockups |
| Demo mode On | row value `On`; no extra warning (the main page info rows say `Demo mode`) |
| Debug log On | note slot visible. If the SD card is missing at the time the page opens, the note reads `No SD card: nothing is logged.` / `Insert a card to log.` (alert icon) |
| HA offline, entity unavailable | not relevant on this page |
| Loading | none |

Refresh: enter and leave = GC16 full; toggles = one partial of the row (INK only, `epd_fastest`); note appear/clear = one partial of the note rect.
