# Settings screen

System screen (not config-driven). Opened by a **long push of the rocker at TAB
level** (layout.md 7.2) or by a **tap on the battery cluster** in the status bar
(hit kind `settings`, layout.md section 6). Shown in the current orientation; no
tab bar, no status bar. Values from `docs/ui/STYLE.md` (revision 3: touch targets
64, gaps 12, every text and icon placed by its centre line `cy`). Mockups (revision 7, task 047): overview `settings-portrait.svg` / `settings-landscape.svg`; category pages `settings-display-*.svg`, `settings-sleep-*.svg`, `settings-network-*.svg`, `settings-tools-*.svg` (each `-portrait` / `-landscape`; plus `settings-tools-states.svg`, task 087). The old `tools-*.svg` are removed. Rect = (x, y, w, h); screen
coordinates unless it says "row-relative".

## 1. Purpose and data sources

Let the user change the device settings (four on this page, two more on the Tools page, section 8), trigger a refresh, and see
read-only diagnostics.

| Item | Source |
|------|--------|
| Orientation, wake mode, refresh interval, idle timeout (main page); demo mode, debug log (Tools page) | `settings::Settings` (lib/settings). Allowed values and cycle order = `kOrientationValues`, `kWakeModeValues`, `kRefreshIntervalMinValues` = {0, 15, 30, 60, 120, 240}, `kIdleTimeoutSValues` = {10, 20, 30, 60}, `demo_mode` (bool, `Off` -> `On`; task 034), `debug_log` (bool, `Off` -> `On`, NVS `dbglog`, default Off; task 041) |
| Wi-Fi, HA connection (Network page; task 047) | `settings::Settings` fields `wifi_profile` (`Auto` or a profile name from secrets.yaml) and `ha_link` (`auto` / `internal` / `external`); hints read the connected profile name and the link in use from lib/net / ha_client |
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
1a. **One edit buffer for all pages** (see section 3).
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
   - Sleep mode, updates while asleep, keep awake: used the next time the device prepares to
     sleep (power module reads the saved settings). The sleep badge (one for all wake modes)
     (STYLE.md 5.5) is the same for every wake mode.
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
   image is never a stale Settings page; the sleep badge is then drawn on
   the tab screen. OPEN-S1.
6. **Touch Wake note** (superseded by 8.2 notes, revision 10). While Sleep mode is Touch or Touch & rocker (in the
   buffer), a note slot shows an alert box `Touch wake shortens battery life. / Button wake is
   recommended.` No numbers are quoted (power is still an estimate in
   ARCHITECTURE.md). The wake row always shows its pips (no warn icon; task 039).
   With Button the note slot is empty.
7. **Value cycling.** Tap or push on a row moves to the next value in the
   `settings.h` array and wraps from the last to the first. There is no
   "previous" (longest cycle is 6 values; Demo mode and Debug log cycle Off -> On -> Off).


## 3. Structure (revision 7)

Settings is a two-level system: the **overview** (main page) lists four **category rows**; each opens a **category page** with the settings of that group. Orientation moved to Display; Wake mode, Refresh every and Idle timeout to Sleep & wake; Demo mode and Debug log to Tools; Network is new.

| Category | Settings (edit buffer fields) |
|----------|-------------------------------|
| Display | Orientation |
| Sleep & wake | Sleep mode, Updates while asleep, Keep awake, Idle timeout, Learn battery limits; battery use bar (8.2, revision 10, task 085) |
| Network | Wi-Fi (`Auto` / profile names from secrets.yaml), HA connection (`Auto` / `Internal` / `External`) |
| Tools | Demo mode, Debug log, Firmware (update row, 8.6; not part of the edit buffer) |

Rule changes: rule 1a now reads "one edit buffer for all pages"; the `unsaved` chip (header, same place) shows on the overview and on every category page; moving between levels (category row, Back, LONG_PUSH on a category page) saves and applies nothing. Rules 2 to 7 are unchanged, with these additions: **Wi-Fi / HA connection** changes apply on leaving Settings (the network session restarts once, after the tab screen is drawn; Demo mode On starts none). The idle auto-leave (60 s) fires from any page and leaves Settings entirely, saving the buffer. **Value cycling** for Wi-Fi: `Auto` -> profile 1 -> ... -> profile N -> `Auto` (list order of secrets.yaml); HA connection `Auto` -> `Internal` -> `External` -> `Auto`.

Shared geometry (unchanged from revision 6): margins 16, gaps 12, header 54 (portrait) / 50 (landscape) with title F40 INK left, `unsaved` chip (404, 11, 120, 32) / (824, 9, 120, 32) F18 PAPER on INK radius 8, header rule 2 px INK, white background, all changing text INK, static captions F18 DIM (grey 4 / #444), OFF hairlines. Back button (276, 880, 248, 64) portrait / (722, 464, 222, 64) landscape, footer button internals as 3.4.

### 3.0 Overview, portrait (540 x 960)

| Element | Rect | cy | Font / grey |
|---------|------|----|-------------|
| Title `Settings`, chip, rule | as above | 27 | |
| Row 1 Display | (16, 68, 508, 96) | | border 2 INK, radius 8, PAPER |
| Row 2 Sleep & wake | (16, 176, 508, 96) | | same |
| Row 3 Network | (16, 284, 508, 96) | | same |
| Row 4 Tools | (16, 392, 508, 96) | | same |
| Info block | (16, 504 ... 808) | | section 3.3, rows 48/48/48/48/64/48 from y = 504 |
| Refresh now | (16, 880, 248, 64) | 912 | 3.4 |
| Back | (276, 880, 248, 64) | 912 | 3.4 |

Category row (row-relative, 508 x 96): label F24 INK left at x = 16, cy = 34; **one-line summary** F18 INK left at x = 16, cy = 66; chevron_right icon (M, 40 x 40) at (W - 52, 28), cy = 48. Summaries (from the edit buffer, so they follow unsaved changes; fit rule F18 then `...`): Display `Portrait` / `Landscape`; Sleep & wake `<Sleep mode>, updates <X>` plus `, USB awake` / `, always awake` when Keep awake is On USB / Always, e.g. `Power off, updates 3 h`, `Deep sleep, updates off`, `Touch & rocker, updates 30 min, always awake` (X = `off` / `30 min` / `1 h` / `3 h` / `6 h` / `12 h`; fit rule: drop the word `updates `, then `...`; revision 10, the idle timeout is no longer in the summary); Network `Wi-Fi <value>, HA <value>` e.g. `Wi-Fi Auto, HA Auto` or `Wi-Fi Hotspot, HA Internal`; Tools `Demo Off, Log Off`, or `Update available, Demo Off, Log Off` while a newer release is known (task 087; same fit rule, then `...`). Vertical sums: header 56 +12 = 68; 4 x 96 + 3 x 12 = 420 -> 488; +16 = 504; info 304 -> 808; footer 880 (72 px of free space between).

Info rows (3.3 table) change only in wording: HA row `internal, ok 14:05` / `external, ok 14:05` (the link in use; was local / remote); Wi-Fi row shows the SSID of the connected profile.

### 3.0.1 Overview, landscape (960 x 540)

Left column rows (16, 64 + 108 k, 456, 96), k = 0..3 (Display 64, Sleep & wake 172, Network 280, Tools 388; last ends 484). Row internals as portrait with W = 456. Right column unchanged: info rows from (488, 64) with heights 56/56/56/56/72/56, value x = 646 (the two-line config value falls to F18 here), Refresh now (488, 464, 222, 64), Back (722, 464, 222, 64).

## 3.1 Setting row (category pages)

Two row types. **Pip rows** (2 to 6 fixed values: Orientation, Wake mode, Refresh every, Idle timeout): exactly the revision 6 row (portrait 508 x 72, landscape 456 x 68; label F24 at x = 16 cy 26/24, pips x = 22 + 20 k cy 49/47, value F40 right edge x = W - 16). **Hint rows** (Wi-Fi, HA connection, Demo mode, Debug log): 88 high; label F24 cy = 32, hint F18 INK cy = 62, value F40 INK right edge x = W - 16, cy = 44; no pips (the value set is open or the row has a hint). Value fit rule for profile names: F40, then F24, then `...` (box 240..492 wide).

Value texts: as revision 6; Wi-Fi `Auto` or the profile name; HA connection `Auto` / `Internal` / `External`; Demo mode, Debug log `Off` / `On`.

### 3.2 Note slot (W x 64): as revision 6 (alert icon M at (12, 12), lines F18 cy 21 / 43), shown only when its condition holds.



### 3.3 Info rows

Each row has a 1 px OFF hairline on its top edge (16, y, 508, 1). Rows in the table are in portrait; landscape has its own table in section 4. Label at
x = 0 (row-relative), F18 DIM (static caption), left, row cy = h/2; value at x = 158, F24 INK, left,
same cy (config row: two lines at h/2 -14 and h/2 +14). When a status icon (S,
24x24) is needed it sits at (158, cy-12) and the value text moves to x = 190. Text
fitting rule: F24, then F18, then `…`.

| Row | Rect | cy | Label | Value |
|-----|------|----|-------|-------|
| Firmware | (16, 504, 508, 48) | 528 | `Firmware` | `v0.3.1` |
| Battery | (16, 552, 508, 48) | 576 | `Battery` | `78%, 3.92 V`; with a learned limit (task 079) `78%, 3.92 V, min 3.88 V`. Fit rule: worst case `100%, 3.92 V, min 3.88 V` is 308 px at F24 (column x 174..524 = 350, or 334 with the icon at x 190 + text at 206: fits in portrait), 229 px at F18 (landscape column x 646..944 = 298 with icon 314: F24 does not fit, so F18 is used by the fit rule). Shorter fallback if ever needed: `78%, 3.92 V, min 3.88` (drop the last `V`). (alert icon if <= 15 %, bolt icon if charging) |
| HA | (16, 600, 508, 48) | 624 | `Home Assistant` | `Demo mode` (demo mode On) / `internal, ok 14:05` / `external, ok 14:05` / alert icon + `offline, last ok 13:05` / alert icon + `offline, never ok` / `-` before the first session |
| Wi-Fi | (16, 648, 508, 48) | 672 | `Wi-Fi` | `Demo mode` (demo mode On), or SSID, or `SSID (not connected)` (SSID of the connected profile), or `-` |
| Config | (16, 696, 508, 64) | 728 (lines 714 / 742) | `Config` | `OK`; or alert icon (cy 702) + first error up to 2 lines: `tabs[0].widgets[2].entity (line 34)`; or warn icon + `no config.yaml on SD`; `built-in demo` while demo mode is On (the SD config is not read; OPEN-S4) |
| Errors | (16, 760, 508, 48) | 784 | `Config errors` | count (`0`, `3`), `-` when the config is missing; `0` in demo mode |

Times: same day `14:05`; earlier day `26.10. 13:05`.

### 3.4 Footer buttons (button-relative, 248 x 64)

| Element | Rect | cy | Font |
|---------|------|----|------|
| Icon (M) | (12, 12, 40, 40) | 32 | `refresh` / `chevron_left`, INK; while a refresh runs the hourglass (M), after a failed one the alert icon (M), in place of the refresh glyph |
| Label | x = 60, box (60, 0, W-68, 64), left | 32 | F24: `Refresh now` / `Back` |

(There is no separate badge: the status shows in the icon slot, so the label
never collides with it. `Refresh now` is about 149 px at F24, ends at x = 209.)


## 5. Interactions (overview)

| Target | Action | Redraw |
|--------|--------|--------|
| Category row (whole rect) | open that category page; no save | full refresh (GC16) |
| Refresh now | start the state fetch (ignored while one runs) | button (pending look: dashed 3 px border, hourglass M); on completion: button, HA row, Battery row |
| Back | leave Settings: save if dirty, apply (rule 2/3) | full refresh |

Rocker, overview: focus stops 1 Display, 2 Sleep & wake, 3 Network, 4 Tools, 5 Refresh now, 6 Back (landscape: same order, left column then right). Entry focus = stop 1. PREV/NEXT move and wrap. PUSH on a row opens the page; on Refresh now starts the fetch; on Back leaves. **LONG_PUSH = back one level**: on the overview = leave Settings (as Back); on a category page = back to the overview. Focus ring: 3 px, 3..6 px outside the rect (ring dirty rect = rect inflated by 6). Key mapping layout.md 7.1. For yes/no dialogs: UP = yes, DOWN = no, PUSH = skip (not used here).

Settings is entered by LONG_PUSH at tab level or a tap on the battery cluster. Targets >= 64 with >= 12 px gaps.

Refreshes: enter / leave Settings and every overview <-> category transition = GC16 full; everything else one partial (INK-only regions `epd_fastest`, regions with DIM labels or hairlines `epd_quality`); 12 feedback refreshes trigger a GC16 clean-up at the next quiet moment.


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
- OPEN-S5: the category summaries (3.0) are proposals.
- OPEN-S6: Wi-Fi profile names longer than about 12 chars shrink to F24 (then `...`); a profile that is out of range stays selectable.
- (resolved) OPEN-S2: the touch path to Settings is a tap on the battery cluster.
- OPEN-S4: in demo mode the Config row text `built-in demo` and the inert Refresh now are proposals (task 034 does not say); architect to confirm.
- OPEN-S3: first line of info only shows the first config error; a scrollable
  list is not specified.
- Questions for the architect: where `FW_VERSION` comes from; whether the config
  module exposes path + line + count for errors.


## 8. Category pages

All pages: header (title = category name, chip when the buffer is dirty), no tab bar, no status bar, Back at the footer position, rows start at y = 68 (portrait) / 64 (landscape). Landscape uses the same left-column rows (456 wide) unless stated.

### 8.1 Display (`settings-display-*.svg`)
Portrait: Orientation pip row (16, 68, 508, 72). Below it a static caption F18 DIM at (16, 152) cy 168: `Applied when Settings is left.` Landscape: row (16, 64, 456, 68), caption at (16, 132) cy 148. Mockup shows the changed value (unsaved), focus on the row.

### 8.2 Sleep & wake (`settings-sleep-*.svg`, revision 10, task 085)
Replaces Wake mode, Refresh every and Stay awake on USB (old 9.1). Mockups: `settings-sleep-portrait.svg` (Power off, updates 3 h, Keep awake Off, focus on Keep awake), `settings-sleep-landscape.svg` (Touch & rocker, 30 min, Keep awake Off, touch note, focus on Updates), `settings-sleep-states.svg` (every value of the mode and Keep awake rows, forced-Off row, notes, four battery use looks (1, 3, 7, 10)). Row types as 3.1; values F40 INK right edge W - 16; labels F24 INK at x = 16 (row-relative); ids w-N as listed.

| # | Row (id) | Type | Portrait rect | Landscape rect |
|---|----------|------|---------------|----------------|
| 1 | Sleep mode (w-1) | hint, 88 | (16, 68, 508, 88) | (16, 64, 456, 88) |
| 2 | Updates while asleep (w-2) | pip, 72 / 68 | (16, 168, 508, 72) | (16, 164, 456, 68) |
| 3 | Keep awake (w-3) | hint, 88 | (16, 252, 508, 88) | (16, 244, 456, 88) |
| 4 | Idle timeout (w-4) | pip, 72 / 68 | (16, 352, 508, 72) | (16, 344, 456, 68) |
| 5 | Learn battery limits (w-5) | hint, 88 (as before: hint `Raise low limit after brownouts`, `Off` / `On`) | (16, 436, 508, 88) | (488, 64, 456, 88) |
| - | Note area (w-6, w-7 when shown) | notes 64 high, stacked, pitch 76, in this order | from (16, 536, 508, 64) | from (488, 164, 456, 64) |
| - | Battery use block (next w id) | 124 high, not interactive | (16, 744, 508, 124) | (488, 328, 456, 124) |
| - | Back (last w id) | 3.4 | (276, 880, 248, 64) | (722, 464, 222, 64) |

Vertical check (portrait): 68 + 88 + 12 + 72 + 12 + 88 + 12 + 72 + 12 + 88 = 524; notes 536..676 (two at most); battery use block 744..868; Back 880 (12 px gap). Landscape: left column ends 412; right column notes end 304 at most; battery use block 328..452; Back 464.

**Row 1, Sleep mode** (hint row; value F40, `Touch & rocker` F24 because F40 does not fit; hint F18 INK, live, label cy +32, hint cy +62, value cy +44). Cycle order = cost order, wraps: `Power off` (hint `Lowest drain, hold 3 s`: from power-off the rocker must be held about 3 s to wake) -> `Deep sleep` (`Rocker, faster wake`) -> `Touch` (`Tap wakes`) -> `Touch & rocker` (`Tap or rocker`). Default `Power off` once task 084 works. Rocker always wakes in the first two and in `Touch & rocker`; `Touch` wakes by tap only. Migration of the old wake mode is firmware (Button -> Deep sleep unless 084 is usable, Touch, Touch & Button).

**Row 2, Updates while asleep** (pip row, 6 pips): `Off` / `30 min` / `1 h` / `3 h` / `6 h` / `12 h` (cycle, wraps; 0 = Off). **Forced Off**: when Sleep mode is `Power off` and 084 cannot make the RTC power-on work, the row takes the unavailable look (STYLE.md section 8: 2 px dotted OFF border, label in OFF grey, no pips, value `Off` INK), ignores tap and PUSH, is skipped by the rocker focus, and the note `This device can't power on by` / `itself: updates stay Off.` shows (first slot). The stored value is kept and returns when the mode changes.

**Row 3, Keep awake** (hint row, value F40): `Off` / `On USB` / `Always`. Live hint: Off `Sleeps as set above`; On USB `Awake while on USB`; Always `Never sleeps`. The cycle is `Off` -> `On USB` -> `Always` -> `Off` in every Sleep mode (task 085: the USB probe is reliable); no mode-dependent hint or fallback. Takes effect when Settings is left.

**Row 4, Idle timeout**: unchanged (4 pips, `10 s` / `20 s` / `30 s` / `60 s`). Unused while Keep awake is Always (the row stays editable, no special look).

**Note area.** Alert-icon notes as 3.2 (icon M at (12, 12), lines F18 cy 21 / 43, text x = 64), stacked from the top of the area, at most two, in priority order: (1) forced Off (text above); (2) Sleep mode is `Touch` or `Touch & rocker`: `Touch wake shortens battery life.` / `Rocker wake is recommended.` (replaces the old rule 6 text; no numbers). A note appearing or clearing redraws the whole note area.

**Battery use block** (not a button; no focus stop; replaces the estimate block, no days/weeks text, no estimate wording). Top hairline (0, 0, W, 1) OFF; caption `Battery use` F18 DIM at x = 16, cy = 24; level text `N/10` F40 INK at x = 16, cy = 60; bar of 10 segments at y = 92, h = 16, from x = 16, pitch 46 (portrait, segment 42 wide, gap 4) or 42 (landscape, segment 38, gap 4). Segments 1..N filled INK, the rest 2 px INK outline on PAPER (shape, not grey). Offsets are block-relative. N = 1..10 comes from lib/power (1 = lowest drain, 10 = highest, e.g. Keep awake Always); N is an integer, never 0. Unavailable (model has no constants yet): text `n/10`, all segments outline.
Redraw: the caption and hairline are static; the live part is the level text and bar, region (0, 36, W, 88) block-relative, INK/PAPER only (`epd_fastest`). A tap changes the row (one partial) and this region together as one update; N is recomputed from the shared edit buffer on every change.

**Interactions.** Row tap or PUSH cycles the value in the buffer (rows 1..3, 4, 5); redraw = the row, the battery use region, the note area if its condition flipped, the `unsaved` chip if the dirty flag flipped (partial; INK-only regions `epd_fastest`; the forced-row dotted OFF border makes its redraw `epd_quality`). Rocker focus order: Sleep mode, Updates while asleep (skipped when forced), Keep awake, Idle timeout, Learn battery limits, Back. LONG_PUSH = back to the overview. Focus ring as section 5. Settings are saved on leaving (rule 2) and used the next time the device sleeps; the status-bar indicator (STYLE.md 5.6) shows the result.

**States.** Loading: none. HA offline / entity unavailable: not relevant (device-local settings). Battery use unavailable: see the block. Demo mode: unchanged.

### 8.3 Network (`settings-network-*.svg`)
Hint rows, 88 high. Portrait: Wi-Fi (16, 68, 508, 88), HA connection (16, 168, 508, 88), note slot (16, 268, 508, 64). Landscape: Wi-Fi (16, 64, 456, 88), HA connection (488, 64, 456, 88), note (16, 164, 456, 64) (under Wi-Fi). Hints are live, read from the network state, never edited:
- Wi-Fi hint: `Connected: <profile name>`; `Connected: -` / alert icon not used, text `Not connected` when no profile joined; `Demo mode` text `Not used (demo mode)`.
- HA connection hint: `Link: internal` / `Link: external` (the link in use); when the mode is Auto and the profile decides: `Link: external (profile)`; offline `Link: none (offline)`.
- Note slot (alert icon): only while Wi-Fi is a forced profile (not Auto): `Forced profile: other Wi-Fi` / `networks are not tried.` When HA connection is forced and the other link would be the profile's choice, no note (the hint shows the link).
Mockups: Wi-Fi forced `Hotspot` (note visible), HA connection `Auto` resolving to external, focus on HA connection.
Effect: Wi-Fi / HA connection are applied on leaving Settings (rule 3 addition); the hints show the state from when the page opened (snapshot, not live).

### 8.4 Tools (`settings-tools-*.svg`)
Hint rows 88 high: Demo mode, Debug log (portrait (16, 68) and (16, 168), landscape (16, 64) and (488, 64)); Debug log note slot (portrait (16, 268, 508, 64), landscape (488, 164, 456, 64)): `Writes /debug.log (max 1 MB).` / `Uses more battery.`, or with no SD card `No SD card: nothing is logged.` / `Insert a card to log.` Hints: `Sample dashboard, offline`, `Log lines to SD card`.

Task 087 adds the **Firmware row** (8.6) as the third row: portrait (16, 268, 508, 88), the Debug log note slot moves to (16, 368, 508, 64); landscape (16, 164, 456, 88) under Demo mode (note slot unchanged). Focus order: Demo mode, Debug log, Firmware (only when actionable), Back. Mockups: portrait shows update available, landscape up to date; all states in `settings-tools-states.svg`.

### 8.5 Interactions and rocker (all category pages)
| Target | Action | Redraw |
|--------|--------|--------|
| Row | cycle (3.1 value order) / toggle in the shared buffer | row value (+ hint); note slot if its condition flipped; chip if dirty flipped (partial) |
| Back | up to the overview (no save, no apply); overview focus = the category row just left | GC16 full |

Rocker: focus stops on a category page = its rows top to bottom (landscape Network and Tools: left to right), then Back. Entry focus = stop 1. PREV/NEXT move and wrap. PUSH on a row cycles; on Back returns. **LONG_PUSH = back one level** (category page -> overview; overview -> leave Settings and save). Ring as section 5. Targets >= 64 (rows 68..88, Back 64), >= 12 px apart. Idle auto-leave: see section 3.

States on category pages: Normal as the mockups; Loading none; Network offline: hint `Link: none (offline)` and the rows stay editable; entity unavailable: not relevant.
Refresh: entering and leaving a page = GC16 full; row toggles = one partial of the row (INK only, `epd_fastest`); note appear/clear = partial of the note rect.


## 9. Stay awake on USB and Sleep now (revision 8, task 081)

Mockups: `settings-portrait.svg` / `settings-landscape.svg` (overview, Sleep now row), `settings-sleep-portrait.svg` / `settings-sleep-landscape.svg` (toggle row, shown On, unsaved). Placement decision: the toggle is a sleep setting, so it lives on the **Sleep & wake** page (not the overview, which has no room for two more rows); only the action **Sleep now** is on the overview.

### 9.1 Stay awake on USB (superseded)
Replaced by Keep awake (`Off` / `On USB` / `Always`) in 8.2 (revision 10, task 085). The row layout, the `Never sleeps while on USB power` hint and the old focus order no longer apply. The OPEN-S7 hint question is answered by the status-bar indicator (STYLE.md 5.6).

### 9.2 Sleep now (overview)
| Element | Portrait | Landscape | Font / grey |
|---------|----------|-----------|-------------|
| Row (button look, as Refresh now) | (16, 804, 508, 64) | (488, 388, 456, 64) | border 2 INK, radius 8, PAPER |
| moon icon (M, 40 x 40) | (28, 816) | (500, 400) | INK |
| Label `Sleep now` | x 76, cy 836 | x 548, cy 420 | F24 INK |
| Right hint `or hold push 2.5 s` (static) | right edge x 508, cy 836 | right edge x 928, cy 420 | F18 INK |

The hint teaches the rocker shortcut (hold PUSH >= 2.5 s sleeps from anywhere, also main screen and popups). Because the row had to fit, the info block is tighter: portrait rows 44/44/44/44/64/48 from y = 504 (ends 792; Sleep now follows at +12); landscape rows 48/48/48/48/72/48 from y = 64 (ends 376). Info text positions stay row-centred (3.3 rules), Config two lines at cy -14 / +14. Footer buttons unchanged. Ids: w-5 = Sleep now, w-6 Refresh now, w-7 Back.

Interactions: tap or PUSH on the row = the same path as the idle auto-leave (rule 5): leave Settings (save if dirty, apply orientation/demo/network), draw the tab screen with a full refresh, then the normal sleep path (sleep badge, wake sources, refresh alarm per wake settings; the refresh alarm is skipped only while idling on USB with the setting On; Sleep now itself always sleeps normally). No confirm dialog (waking is cheap, a hold is as unguarded). Rocker focus order on the overview: 1 Display, 2 Sleep & wake, 3 Network, 4 Tools, 5 Sleep now, 6 Refresh now, 7 Back (landscape: left column, then Sleep now, Refresh now, Back). No pending look (the device is sleeping within about 2 s). States: demo mode, offline, etc. do not change the row; it is always enabled.

### 9.3 Hold-to-sleep feedback
No on-screen countdown (partial refresh while holding would ghost and cost battery). Feedback is the sleep sequence itself: tab screen full refresh + sleep badge. Holds of 600 ms to < 2.5 s keep the long-push meaning (open/leave Settings, close popups).

### 9.4 OPEN
- OPEN-S7: no USB-power indicator on the toggle row; Battery info row already shows the bolt icon when charging. Add a hint `USB power detected` only if task 081 finds a reliable signal (architect).
- OPEN-S8: toggle placed on the Sleep & wake page, not on the overview (coordinator's wording suggested the overview).

### 10. OPEN (revision 10, task 085)
- OPEN-S9: closed (task 085 notes): the estimate is replaced by the 1..10 Battery use bar; architect to confirm lib/power can give a level 1..10.
- OPEN-S11: forced-Off Updates (Power off without a working RTC power-on) shows as a dotted row plus note; confirm 084 can report "RTC power-on unsupported" at settings time.


## 8.6 Firmware update (revision 11, task 087; `settings-tools-states.svg`, ids w-1..w-N as in the sheet)
Settings only: nothing on tab screens, no popup. Data: NVS found version / last-check time (lib ota_logic), `FW_VERSION`, battery %, USB (usb_rx), Wi-Fi state, demo flag. No HA entity.

**Firmware row** (hint-row geometry 3.1, 88 high, not edited, never touches the edit buffer or the `unsaved` chip). Label `Firmware` F24 (x 16, cy 32; OFF grey when refused); value = installed version F40 INK right edge W - 16 cy 44 (F24 if a long version would meet the hint); hint F18 INK x 16 cy 62 (x 48 after an alert icon S 24 at (16, 50)); hint at most about 30 characters. Chip `NEW` row-relative (144, 16, 72, 32), F18 PAPER on INK, radius 8, shown whenever a newer release is known. Row looks (shape carries the state):
| State | Border | Hint | Tap / focus |
|-------|--------|------|-------------|
| Up to date / never checked | 2 px INK solid, no chip (tappable) | `Checked 26.10. Tap to check` (same day `14:05`) / `Not checked. Tap to check` | starts Check now (below); focus stop |
| Checking | 3 px dashed INK (pending) | `Checking...` | ignored, no focus stop; other rows and Back also ignore input until the result |
| Check failed | 2 px INK solid + alert icon | `Check failed: no Wi-Fi` / `no server` / `timeout` | starts Check now again |
| Update available | 2 px INK solid, chip | `Update to 1.3.0 available` | opens confirm |
| Refused | 2 px dotted OFF (STYLE 8 unavailable), chip, label OFF | first reason that applies: `1.3.0: leave demo mode first`, `1.3.0: Wi-Fi not connected`, `1.3.0 needs USB or 50% battery` | ignored, no focus stop (the hint is the explanation) |
| Failed | as Update available + alert icon | `Download failed, try again` / `File damaged, not installed` (SHA-256); old firmware untouched | opens confirm (retry) |
| Updated (first Settings visit after the reboot only; cleared when Settings is left) | info look | `Updated from 1.2.1` (value = new version) | ignored |
| Rolled back (after 3 unhealthy boots) | info look + alert icon | `1.3.0 failed, rolled back` | ignored; that version is not offered again (OPEN-S12) |
Overview: Tools row summary gains `Update available, ` first (3.0); no other overview change (the Firmware info row keeps `v1.2.1`).

**Confirm step** (full page, replaces the Tools page; header `Update`, `unsaved` chip only if dirty; STYLE 10b). Portrait (origin 0,0): `Install 1.3.0?` F40 (16, cy 100); info rows (3.3 pattern, 48 high, hairline OFF, label F18 DIM x 16, value F24 INK x 174) at y 144 / 192 / 240: `Installed 1.2.1`, `New 1.3.0`, `Download 1.6 MB` (size from the manifest); lines F18 INK at x 16, cy 330 / 354 / 378: `Takes about a minute, then the` / `device restarts. Your settings and` / `config are kept.`; alert note 3.2 at (16, 410, 508, 64): `Keep it on USB or charged.` / `Do not switch it off.`; buttons 508 x 64, border 2 INK, label F24 x 16, key hint F18 right x W - 16, cy 32: `Yes, install` `UP` at (16, 696), `No, cancel` `DOWN` (16, 772), `Skip` `PUSH` (16, 848). Landscape: title cy 90, info rows (16, 124 / 168 / 212, 456, 44), lines cy 282 / 306 / 330, note (16, 364, 456, 64), buttons 456 x 64 at x 488, y 200 / 276 / 352. Touch: the three buttons; rocker: UP yes, DOWN no, PUSH or LONG_PUSH skip (no focus ring). No / Skip = back to the Tools page. Entering and leaving = GC16 full. Yes: if the buffer is dirty it is saved first (the reboot must not lose it), then the Tools page is drawn (GC16 full) with the firmware row in progress look, and the download starts.

**Progress** (the Tools page, nothing else changes; row only). Row border 3 px dashed INK (pending look, STYLE 9). Label F24 cy 32 `Downloading 1.3.0`; value F40 INK right edge, cy 36, `42%`; bar row-relative (16, 56, 476, 16): 2 px INK outline on PAPER, fill INK from x 18, 12 high, width = 472 x percent (shape, no grey). Phases: `Downloading 1.3.0` + percent; `Checking 1.3.0` + `100%` (SHA-256); `Restarting` (no value, bar full) -> reboot. Other rows keep their look but ignore touch and rocker (no Back, no cancel); idle/wake sleep is suspended. Redraw: value + bar region row-relative (16, 14, 476, 58), INK only, `epd_fastest`, one partial per 5 % step (or at most every 2 s); label changes = one partial of the label. No other region is touched.
Failure: row returns to its Failed look (one partial), then one GC16 clean-up of the page; the running firmware is untouched and input is active again. Success: after `Restarting` the device reboots into the new firmware (normal cold-boot splash path, then the tab screen); the next Settings visit shows `Updated from ...` once.

**States.** Loading: the row shows the stored result of the last check; a check runs while Settings is open only after Check now (Checking look). HA offline: not relevant; Wi-Fi down: Refused look. Demo mode On: Refused (`leave demo mode first`). Unavailable entity: n/a.

**Check now (OPEN-S13 resolved).** The up-to-date Firmware row is tappable (rocker: focus stop between Debug log and Back, PUSH = tap). Tap: row goes to Checking (partial of the hint, `epd_fastest`), the device fetches the release manifest (needs Wi-Fi; Refused rows for demo/no Wi-Fi stay refused, so Wi-Fi-down shows `Check failed: no Wi-Fi` only if it drops mid-check). Result, one partial of the row: newer release = Update available look (NEW chip, opens confirm); none = `Up to date, checked 14:05`; error = Check failed look. The result is stored like the weekly check. Timeout 20 s. In mockup `settings-tools-states.svg` (w-1, w-21..w-23). Landscape: same row geometry (456 wide).

**OPEN-S12:** after a rollback the failed version should be suppressed (architect).
