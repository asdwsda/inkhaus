# Settings screen

System screen (not config-driven). Opened by a **long push of the rocker at TAB
level** (layout.md 7.2) or by a **tap on the battery cluster** in the status bar
(hit kind `settings`, layout.md section 6). Shown in the current orientation; no
tab bar, no status bar. Values from `docs/ui/STYLE.md` (revision 3: touch targets
64, gaps 12, every text and icon placed by its centre line `cy`). Mockups (revision 7, task 047): overview `settings-portrait.svg` / `settings-landscape.svg`; category pages `settings-display-*.svg`, `settings-sleep-*.svg`, `settings-network-*.svg`, `settings-tools-*.svg` (each `-portrait` / `-landscape`). The old `tools-*.svg` are removed. Rect = (x, y, w, h); screen
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


## 3. Structure (revision 7)

Settings is a two-level system: the **overview** (main page) lists four **category rows**; each opens a **category page** with the settings of that group. Orientation moved to Display; Wake mode, Refresh every and Idle timeout to Sleep & wake; Demo mode and Debug log to Tools; Network is new.

| Category | Settings (edit buffer fields) |
|----------|-------------------------------|
| Display | Orientation |
| Sleep & wake | Wake mode, Refresh every, Idle timeout |
| Network | Wi-Fi (`Auto` / profile names from secrets.yaml), HA connection (`Auto` / `Internal` / `External`) |
| Tools | Demo mode, Debug log |

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

Category row (row-relative, 508 x 96): label F24 INK left at x = 16, cy = 34; **one-line summary** F18 INK left at x = 16, cy = 66; chevron_right icon (M, 40 x 40) at (W - 52, 28), cy = 48. Summaries (from the edit buffer, so they follow unsaved changes; fit rule F18 then `...`): Display `Portrait` / `Landscape`; Sleep & wake `<wake>, <refresh>, <idle>` e.g. `Touch, 60 min, 20 s` (wake `Button` / `Touch` / `Touch & Button` shortened to `Both` only if it does not fit); Network `Wi-Fi <value>, HA <value>` e.g. `Wi-Fi Auto, HA Auto` or `Wi-Fi Hotspot, HA Internal`; Tools `Demo Off, Log Off`. Vertical sums: header 56 +12 = 68; 4 x 96 + 3 x 12 = 420 -> 488; +16 = 504; info 304 -> 808; footer 880 (72 px of free space between).

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
| Battery | (16, 552, 508, 48) | 576 | `Battery` | `78%, 3.92 V` (alert icon if <= 15 %, bolt icon if charging) |
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

### 8.2 Sleep & wake (`settings-sleep-*.svg`)
Pip rows Wake mode (68), Refresh every (152), Idle timeout (236), then the touch-wake note slot (16, 320, 508, 64) (empty with Button). Landscape: rows at y = 64, 144, 224 (h 68), note (16, 304, 456, 64). Mockups: Wake Touch (note visible), focus on Refresh every.

### 8.3 Network (`settings-network-*.svg`)
Hint rows, 88 high. Portrait: Wi-Fi (16, 68, 508, 88), HA connection (16, 168, 508, 88), note slot (16, 268, 508, 64). Landscape: Wi-Fi (16, 64, 456, 88), HA connection (488, 64, 456, 88), note (16, 164, 456, 64) (under Wi-Fi). Hints are live, read from the network state, never edited:
- Wi-Fi hint: `Connected: <profile name>`; `Connected: -` / alert icon not used, text `Not connected` when no profile joined; `Demo mode` text `Not used (demo mode)`.
- HA connection hint: `Link: internal` / `Link: external` (the link in use); when the mode is Auto and the profile decides: `Link: external (profile)`; offline `Link: none (offline)`.
- Note slot (alert icon): only while Wi-Fi is a forced profile (not Auto): `Forced profile: other Wi-Fi` / `networks are not tried.` When HA connection is forced and the other link would be the profile's choice, no note (the hint shows the link).
Mockups: Wi-Fi forced `Hotspot` (note visible), HA connection `Auto` resolving to external, focus on HA connection.
Effect: Wi-Fi / HA connection are applied on leaving Settings (rule 3 addition); the hints show the state from when the page opened (snapshot, not live).

### 8.4 Tools (`settings-tools-*.svg`)
Hint rows 88 high: Demo mode, Debug log (portrait (16, 68) and (16, 168), landscape (16, 64) and (488, 64)); Debug log note slot (portrait (16, 268, 508, 64), landscape (488, 164, 456, 64)): `Writes /debug.log (max 1 MB).` / `Uses more battery.`, or with no SD card `No SD card: nothing is logged.` / `Insert a card to log.` Hints: `Sample dashboard, offline`, `Log lines to SD card`.

### 8.5 Interactions and rocker (all category pages)
| Target | Action | Redraw |
|--------|--------|--------|
| Row | cycle (3.1 value order) / toggle in the shared buffer | row value (+ hint); note slot if its condition flipped; chip if dirty flipped (partial) |
| Back | up to the overview (no save, no apply); overview focus = the category row just left | GC16 full |

Rocker: focus stops on a category page = its rows top to bottom (landscape Network and Tools: left to right), then Back. Entry focus = stop 1. PREV/NEXT move and wrap. PUSH on a row cycles; on Back returns. **LONG_PUSH = back one level** (category page -> overview; overview -> leave Settings and save). Ring as section 5. Targets >= 64 (rows 68..88, Back 64), >= 12 px apart. Idle auto-leave: see section 3.

States on category pages: Normal as the mockups; Loading none; Network offline: hint `Link: none (offline)` and the rows stay editable; entity unavailable: not relevant.
Refresh: entering and leaving a page = GC16 full; row toggles = one partial of the row (INK only, `epd_fastest`); note appear/clear = partial of the note rect.
