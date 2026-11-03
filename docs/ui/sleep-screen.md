# Sleep screen (task 101)

Last frame drawn before sleeping when Settings > Sleep & wake > While asleep = `Sleep screen` (settings.md 8.2). With `Dashboard` nothing changes (tab screen + sleep pill, STYLE.md 5.6). Values from STYLE.md: greys INK 0 / DIM 4 (static captions) / OFF 8 (hairlines, unavailable icon) / PAPER 15; F18 / F24 / F40 / F56; icons S 24, M 40, L 48; margin 16; every text and icon placed by its centre line `cy`; text box = 1.2 x size; value + unit runs share a baseline (`baseline_left`, unit F24 after F40, x + 2). Mockups (PNG in docs/ui/png/; accepted by the user 2026-11-03, may be revised after the device look):

| | all optional blocks | none | no SHT30 + weather unavailable |
|---|---|---|---|
| Portrait 540x960 | `sleep-screen-portrait.svg` | `sleep-screen-portrait-none.svg` | `sleep-screen-portrait-fallback.svg` |
| Landscape 960x540 | `sleep-screen-landscape.svg` | `sleep-screen-landscape-none.svg` | `sleep-screen-landscape-fallback.svg` |

## 1. Purpose and data
A frozen screen that says what it is: a sleeping device that was last updated at a known time. **No live clock** (e-ink changes only on wakes, a clock would lie): the big date is the date at the last wake; `updated HH:MM` is the stale marker.

| Item | Source |
|------|--------|
| Date, `updated`, `next update` | RTC at the last wake; next scheduled RTC wake (as the pill, STYLE 5.6); `no scheduled updates` when Updates while asleep is Off (incl. forced Off) |
| Battery % / voltage | M5Unified power API (as the status bar); `charging` replaces the voltage when USB charging |
| Indoor temperature, humidity | SHT30 reading of this wake (task 100); local, independent of Report to HA |
| Weather (optional) | config `sleep_screen: weather: <weather entity>`; state + `temperature` from the same state fetch of the wake |
| Next event (optional) | config `sleep_screen: calendar: <calendar entity>`; its `message` and `start_time` attributes (HA exposes the next event as the entity state) |
| Notifications (optional) | config `sleep_screen: notifications: true`; count and title of the newest `persistent_notification.*` in the state fetch |

No extra HA request beyond the existing wake fetch (the entities are added to it). Date format: weekday name, day, month name in English, no year (`Tuesday` / `3 November`).

## 2. Layout rules
No status bar, no tab bar, no touch targets. Blocks are separated by a 1 px OFF hairline in the middle of a 24 px gap. Each block has the id `w-N` for the checker; block frames are the block rect plus 12 px.

**Blocks** (width W = 508 portrait, 440 landscape left column, 456 landscape right column; origin (x, y), all offsets block-relative; x of text = block x):

| id | Block | h | Content (font, cy, grey) |
|----|-------|---|--------------------------|
| w-1 | Date | 148 | weekday F56 cy 38, date F56 cy 110, INK, left (boxes 4.4..71.6 and 76.4..143.6) |
| w-2 | Updated | 84 | `updated 14:05` F40 cy 24; `next update 15:30` F24 cy 70, INK. Earlier day: `updated 26.10. 14:05` drops to F24 (fit rule: F40, then F24) |
| w-3 | Battery / Indoor | 128 | two columns, column 2 at x + 260 (portrait) / x + 224 (landscape). Captions F18 DIM cy 11: `Battery`, `Indoor`. Col 1: battery icon at 2x STYLE 5.4 (body 72x40 INK, inner 64x32 PAPER, level `round(64*pct/100)`, nub (72, 10, 8, 20)), centre cy 52, plus `78%` F40 at x + 92 cy 52; `3.92 V` F24 cy 104 (`charging` when charging). Col 2: `21.5` F40 + `°C` F24 on one baseline (cy 52); drop icon S at (col x, cy 104) + `48` F40 + `%` F24 at col x + 32 (baseline from cy 104). Integer for >= 100 %, one decimal for temperature |
| w-4 | Weather (optional) | 84 | caption `Weather` F18 DIM cy 11; condition icon L (48) at (x, cy 54); temperature `12` F40 + `°C` F24 at x + 64 cy 54; condition text F24 INK at x + 160 cy 54 (humanised: `Partly cloudy`, `Rainy`, ...; truncated with `…` at W) |
| w-5 | Next event (optional) | 108 | caption `Next event` F18 DIM cy 11; start time `16:30` F40 at x cy 54; title F24 at x + 132 cy 54, truncated with `…` at W; day line F18 INK cy 94: `Today` / `Tomorrow` / weekday. All-day event: `all day` F24 in place of the time (title then at x + 132 as well). No event: `Nothing planned` F24 at x cy 54 |
| w-6 | Notifications (optional) | 84 | caption `Notifications` F18 DIM cy 11; count F40 at x cy 54; newest title F24 at x + 44 cy 54, truncated with `…`. **Count 0: the block is not drawn** (nothing to say) |
| w-7 | Footer | 56 | hairline above; moon icon + `Asleep` F24 + wake hint F18 (section 3) |

**Portrait (540x960).** One column x = 16, W = 508, from the top: core blocks w-1, w-2, w-3, then the shown optional blocks in the order w-4, w-5, w-6, pitch = h + 24. The whole group is centred vertically in (16 .. 860): `top = 16 + 2 * floor((844 - H) / 4)`, H = sum of heights + 24 per gap. Footer: hairline (16, 876, 508, 1), block (16, 892, 508, 56): moon M (40) at (16, 900), `Asleep` F24 x 68 cy 906, hint F18 x 68 cy 936.
- All blocks (H = 756): y = 60, 232, 340 (core), 492, 600, 732 (ends 816).
- None (H = 408): y = 234, 406, 514 (ends 642); the date sits about a third from the top, the white space is above and below on purpose.
- One or two blocks: same rule, centred (no mockup needed).

**Landscape (960x540).** Two columns, vertical hairline (472, 16, 1, 448) OFF. Left x = 16, W = 440; right x = 488, W = 456; both centred in y 16..464 with the same rule (`top = 16 + 2 * floor((448 - H) / 4)`). Footer: hairline (16, 476, 928, 1), one line, cy 512: moon S (24) at (16, 500), `Asleep` F24 x 52, hint F18 right-aligned to x = 944.
- With optional blocks: left w-1, w-2, w-3 (H 408: y = 36, 208, 316, ends 444); right the shown optional blocks (all three: H 324: y = 78, 186, 318, ends 402).
- None: left only w-1 (y = 166); right w-2, w-3 (y = 122, 230, ends 358). The core is split so both columns are used.
- Block w-3 column 2 starts at x + 224 (264 left column, 712 right column).

## 3. Footer wake hint (F18 INK, from the saved Sleep mode)
| Sleep mode | Hint |
|------------|------|
| Power off | `Hold the side button for 3 s` |
| Deep sleep | `Press the side button` |
| Touch | `Tap the screen` |
| Touch & rocker | `Tap or press the side button` |
The mockups show Touch & rocker. Not a button: touching it does nothing special (a tap wakes).

## 4. States
| State | Look |
|-------|------|
| No SHT30 reading (sensor missing or read failed) | `-` F40 INK in place of the temperature (no unit) and of the humidity (drop icon stays, no `%`); layout unchanged (fallback mockups). Always `-`, never an old value |
| Weather unavailable (`unavailable`, `unknown`, entity missing, fetch failed) | icon `warn` L in OFF grey, temperature `-`, condition `n/a` (INK); block kept so the layout stays |
| Calendar unavailable | `Calendar n/a` F24 in the title position, time and day line omitted |
| Fetch failed on the last wake (HA unreachable) | `updated` shows the last good time; nothing else changes (stale is visible from the time); `offline` is not spelled out. Date stays the date of the wake |
| Never fetched, cache empty | `updated never`; optional blocks show their unavailable looks |
| Battery <= 15 % | the level fill is replaced by `!` as in STYLE 5.4 (2x), percent text unchanged |
| Demo mode | sample values from the demo config; no optional blocks unless the demo config has `sleep_screen:` |
| Loading | n/a (drawn once, after the fetch) |

## 5. Interactions
None on the screen. Wake by the source in the Sleep mode (touch or rocker) shows the dashboard tab that was active (task 086) with the normal wake refresh; the wake tap is consumed (layout.md OPEN-3). The sleep pill is **not** drawn on the sleep screen (the footer replaces it).
Rocker: not applicable while asleep.

## 6. Refresh
- Entering sleep: **one GC16 full refresh** of the whole screen (it holds DIM captions, OFF hairlines and the OFF warn icon, and the content is new), then the EPD powers off. No partial draw.
- Scheduled wake with changes (`updated`, `next update`, battery, climate, weather...): redrawn as a GC16 full (task 092 rule); nothing changed = no redraw (the old frame stays, including the old `updated` time).
- Wake by touch/rocker: the dashboard is drawn as the normal first wake refresh (full, content is new). Nothing is drawn on top of the sleep screen.
- Ghost debt: the screen is always a full refresh, so none carries over.

## 7. Open design questions (recommended defaults used above)
1. English date, no year, weekday on its own line: OK?
2. Notifications = HA persistent notifications (count + newest title); block hidden at 0.
3. Weather icon set: the L icons of STYLE 8 (mockups use the `cloud` shape as placeholder for every condition).
4. The footer replaces the sleep pill: OK, or draw the pill (mode word) as well?
