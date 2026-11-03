# Guide visuals (task 111)

Owner: ui-designer. Plan and style for every image in the user guide (website `site/`, `docs/guide/`).
Shared values (grid, fonts, greys, icons) are in [STYLE.md](STYLE.md); widget geometry in [widgets.md](widgets.md)
and [widgets-new.md](widgets-new.md); screens in [settings.md](settings.md), [sleep-screen.md](sleep-screen.md),
[layout.md](layout.md). Images reuse the accepted mockups 1:1 (widget `<g>` groups are copied out of the
`docs/ui/*.svg`, never redrawn), so they match the device.

Status column: **done** = drawn and rendered in this batch (batch 1), blank = to draw (batches 2..4, see the end).

## 1. Pipeline and style

- Sources: `docs/guide/img/src/<name>.svg` (960-wide screens are kept at 1:1; fragments are cropped to their own
  canvas). `docs/guide/img/gen_sources.py` writes the batch-1 sources from the accepted mockups
  (`python3 -I docs/guide/img/gen_sources.py`); later images extend it (one function per image kind).
- Render: `tools/render_guide_images.sh [name ...]` -> `docs/guide/img/<name>.png` (Inkscape, DejaVu Sans and
  DejaVu Sans Mono, 96 dpi = 1:1 px, converted to 16 grey levels, metadata stripped). Rerunning reproduces the PNGs
  byte-identically (checked). Limit 60 KB per PNG (batch 1: 7 to 41 KB).
- Lint: `tools/check_mockups.py docs/guide/img/src/<name>.svg` for every image that contains widgets (ids `w-N`,
  `w-N-frame`, `w-N-*`). Images without widgets (device drawing, diagrams) report "no widget frames" and are
  checked by eye only; diagram shapes carry ids with prefixes `dg-`, `lg-`, `co-` so a later lint can cover them.
- Figure kinds and look:
  - **Screen shot**: the real 960x540 (or 540x960) mockup, thin `#888` edge, 40 px margin, numbered badges
    (white disc, 3 px black ring, bold 20 px number) with a numbered legend below (F18, one line each, max 60
    characters). Dashed 3 px black rectangles mark regions that are not a widget.
  - **Fragment** (widget gallery): 20 px margin, the widgets at 1:1, an 18 px `#444` label under each widget
    ("on (filled)", "size: 2x1"), then a config panel: `#f2f2f2` fill, `#888` 2 px border, radius 8, tag
    `config.yaml`, DejaVu Sans Mono 20 px, comment lines `#444`. The snippet is exactly what produces the widget
    (entity names are the sample ones from `config.example.yaml`). Width = widest of widgets and snippet.
  - **Diagram**: boxes with 2 px black border, radius 8, 20 px text, arrows 2 px with filled triangle heads,
    max 1000 px wide, only greys 0/4/8/15 plus `#f2f2f2` panels (web only, never drawn on the device).
  - **Device drawing**: vector line art, 3 px body, shapes by fill (black = button/port), "approximate" note.
- Text in images is English, sentence case, no emoji. Alt text: one sentence, says what the picture proves
  ("Light widget on (filled black) and off (outline), with the two config lines that make them"), under 140 chars.
- The website may show images at 100% width up to 1000 px; PNGs are 1:1 so they look sharp when displayed at
  less than or equal 1x on a normal screen and at 2x are still readable (smallest text 16 px).
- Later (device): replace gallery fragments by real `shot` screenshots where they differ.

## 2. Image list

Columns: file (no extension; `.svg` in `src/`, `.png` in `img/`), what it shows, caption (guide text under the
image), alt text, guide section. Captions and alt texts are drafts for step 110.

### A. Meet the device (section "Getting to know the device")

| File | Shows | Caption | Alt | Status |
|---|---|---|---|---|
| `device-buttons` | Front and back line drawing: screen, rocker (UP / PUSH / DOWN) with its actions, USB-C, microSD, reset pinhole | The rocker and the ports. Positions are approximate. | Line drawing of the M5Paper front with the rocker on the right edge and the back with reset and SD slot. | done |
| `device-gestures` | Cheat sheet: tap, long press (history graph), rocker up/down/push/hold, tap "updated", tap battery; one pictogram per row (hand circle icon, rocker bar) | Everything you can do without a keyboard. | Table of gestures with a small icon each. | |
| `device-yesno` | A confirm dialog with UP = yes, DOWN = no, PUSH = skip marked on a rocker sketch | In a yes/no question the rocker answers. | Confirm dialog with the rocker keys labelled yes, no and skip. | |
| `device-focus-ring` | Tab screen with the rocker focus ring on one widget (3 px ring) and the three rocker steps as a mini sequence | The ring shows what PUSH will activate. | Widget with a thick ring around it and three arrows for the rocker. | |
| `device-orientation` | Device portrait and landscape, rocker on the right and on top, "UP = previous" arrows | Landscape turns the screen, the rocker keeps its meaning: up points left. | Two device sketches, portrait and landscape, with the rocker position. | |

### B. Install and first start (section "Install")

| File | Shows | Caption | Alt | Status |
|---|---|---|---|---|
| `install-flow` | 5 boxes with arrows: Web flasher -> SD card files -> Insert and power on -> Home Assistant token -> First screen | Five steps from box to dashboard. | Flow of five install steps. | |
| `install-flasher-steps` | The three screens of the web flasher (browser chrome simplified): Connect, Install, Done (from web-flasher mockups) | The web flasher in three clicks. | Three web flasher screens in order. | |
| `install-sdcard-tree` | File tree of the SD card root: `secrets.yaml` (private, red-free: lock icon), `config.yaml`, optional `sleep.png`/art folder; one-line purpose each | What goes on the card. | Tree of the SD card files with a one-line purpose each. | |
| `install-two-files` | Two panels: `secrets.yaml` (Wi-Fi, HA URL, token; "private, never share") and `config.yaml` (tabs, widgets; "safe to share") with example excerpts | Two files: the private one and the shareable one. | Side by side view of secrets.yaml and config.yaml with their contents. | |
| `install-token` | Home Assistant profile page schematic: Security tab -> Long-lived access tokens -> Create token -> copy; numbered 1..4, no real HA screenshot (changes between versions) | Create a long-lived access token. | Four numbered steps in the Home Assistant profile page. | |
| `install-first-boot` | Four frames at 25% scale: splash, "Connecting", tab screen with "updated" label, last frame "first full redraw" | What you see on the first start. | Four small screens: splash, connecting, first tab, redraw. | |
| `install-minimal-config` | Result picture: a 4-line `config.yaml` (one tab, one light, one sensor, one cover) next to the 960x540 screen it makes | The smallest useful config and the screen it produces. | Four line config on the left, resulting tab screen on the right. | |
| `install-wifi-profiles` | Up to 4 Wi-Fi profiles in priority order, arrow "first one in range wins", HA URL internal / external / auto | Wi-Fi profiles and which HA address is used. | Diagram of Wi-Fi profiles with internal and external URL. | |

### C. The screens (section "Using the dashboard")

| File | Shows | Caption | Alt | Status |
|---|---|---|---|---|
| `screen-tab-anatomy` | Landscape tab 1:1 with 7 badges: status label, tabs, last slot (sleep pill sample), battery, filled/outline, buttons, free cells | The tab screen, part by part. | Landscape tab screen with seven numbered callouts. | done |
| `screen-tab-anatomy-portrait` | Same for portrait (status bar on top, tab bar at the bottom, 2 columns x 7 rows) | The same screen held upright. | Portrait tab screen with numbered callouts. | |
| `screen-popup-anatomy` | Light brightness popup with callouts: title, close, plus/minus, presets, Off; unsent value pending icon | A popup: tap a light's name area to dim it. | Brightness popup with numbered parts. | |
| `screen-popup-history` | History popup (line graph) with callouts: period, y axis, min/max, close | Long press a sensor to see its history. | History graph popup with callouts. | |
| `screen-status-icons` | Table of status-bar symbols at 1:1: Wi-Fi, no Wi-Fi, outdated clock, battery levels (100/50/10/charging), warn; meaning each | What the little symbols mean. | Table of status bar symbols with their meaning. | |
| `screen-orientation` | The same tab in portrait (540x960) and landscape side by side at 50% | Portrait or landscape, set in Settings > Display. | Same dashboard shown upright and sideways. | |
| `screen-columns-portrait` | `portrait_columns` 2, 3, 4 side by side at 40% with the same widgets; snippet `portrait_columns: 3` | One setting, three densities. | Three portrait screens with 2, 3 and 4 columns. | |
| `screen-columns-landscape` | `landscape_columns` 5 vs 4 side by side at 50%; wide widgets get two cells at 4 | Five columns by default; four gives wide widgets more room. | Two landscape screens with 5 and 4 columns. | |
| `screen-pager` | A tab with more widgets than fit: sub-page pager "2 / 3" with prev/next, snippet showing 24 widgets | Too many widgets? The tab splits into pages. | Tab screen with a page switcher at the bottom. | |
| `screen-tabs-overflow` | Tab bar with 7 tabs: overflow slot "..." and its list | More tabs than slots. | Tab bar with an overflow slot. | |
| `screen-refresh-what-to-expect` | A frame with ghosting exaggerated (light grey ghost of the previous state) and the cleaned frame; "a flash now and then is normal" | E-ink shows faint traces; a full refresh clears them. | Two views of the same widget, with and without ghosting. | |

### D. Settings (section "Settings")

Each page: the accepted mockup 1:1 with 3 to 6 badges and a legend; the row meanings come from settings.md.

| File | Shows | Caption | Alt | Status |
|---|---|---|---|---|
| `settings-overview` | Overview: 4 category rows with summaries, Refresh now, Back, unsaved chip, info block | Settings: four categories and the device info. | Settings overview with callouts. | |
| `settings-display` | Display page: orientation, brightness, (other rows as in spec) | Display. | Display settings page. | |
| `settings-sleep` | Sleep & wake page: mode, updates, keep awake, while asleep, battery use block | Sleep & wake. | Sleep settings page. | |
| `settings-network` | Network page: Wi-Fi profile, HA connection | Network. | Network settings page. | |
| `settings-tools` | Tools page: Demo mode, Debug log, update, config status row | Tools. | Tools settings page. | |
| `settings-values-cycle` | One row tapped three times: value cycles (Power off -> Deep sleep -> Touch), no list | A tap on a value switches to the next one. | Three frames of one setting changing value. | |
| `settings-leave` | Three ways to leave: Back, long push, idle 60 s; "unsaved" chip -> saved on leaving | Leaving Settings saves. | Diagram of the ways to leave Settings. | |

### E. Sleep, battery and the network (section "Sleep and battery")

| File | Shows | Caption | Alt | Status |
|---|---|---|---|---|
| `sleep-modes` | 4 rows (Power off, Deep sleep, Touch, Touch & rocker): what wakes it, wake time, battery use level 1..10 as filled bars (same meter as the Settings battery use block) | Four sleep modes and what they cost. | Table of sleep modes with wake method and battery use bars. | |
| `sleep-timeline` | Time line: asleep -> wake (RTC / rocker / touch) -> Wi-Fi -> fetch states -> draw -> sleep, with "scheduled update every 3 h" ticks | What happens each time it wakes. | Timeline of one wake cycle. | |
| `sleep-ha-talk` | Device <-> router <-> Home Assistant: REST for states, WebSocket for notifications; internal vs external URL | How the device talks to Home Assistant. | Diagram of device, router and Home Assistant with the calls. | |
| `sleep-pills` | The four sleep pills at 1:1 (Zz replaced by OFF / Deep / Touch / Touch+rocker, next update time) | The pill tells the mode and the next update. | Four pill indicators with their meaning. | |
| `sleep-screen-variants` | Sleep screen (clock, weather, calendar, notifications), the fallback and `none` (last tab frozen), portrait + landscape at 40% | While asleep: three looks. | Three sleep screens side by side. | |
| `sleep-screen-config` | `sleep_screen:` snippet with the block it adds highlighted on the screen (weather / calendar / notifications), one row each | Each line in `sleep_screen` adds one block. | Three snippets, each next to the screen block it adds. | |
| `sleep-battery-levels` | Battery cluster at 100, 78, 50, 25, 10 % and "learning" note | The battery icon and what it reads. | Battery icons at five levels. | |
| `sleep-before-after` | Option comparison: "Keep awake Off" vs "On USB" (screen identical, status bar shows plug), and mode Touch vs Power off wake time | What Keep awake changes. | Two status bars, with and without keep awake. | |

### F. Widget gallery (section "Widgets")

One fragment per row (widgets + states + snippet). Landscape cell widths for 1x1 (176) and wide (552); 2x1 =
364; portrait 1x1 = 248, 2x1 = 508. Fragment name: `gallery-<widget>-<variant>`.

| File | Shows | Caption | Status |
|---|---|---|---|
| `gallery-light-on-off` | light 1x1 on (80 %) and off + snippet with `name:` | A light: filled when on, outline when off. | done |
| `gallery-light-states` | on / off / pending (hourglass) / unavailable (dotted frame, slash) | A light in each state. | |
| `gallery-switch` | switch on/off/unavailable (plug icon) | A switch. | |
| `gallery-automation` | automation enabled/disabled (circular arrows) | An automation: tap to enable or disable. | |
| `gallery-cover-states` | cover opening (up filled), closed, stopped, pending (dashed hourglass), unavailable | A shutter in five states. | |
| `gallery-cover-group` | `groups:` entry + `group:` widget with `name:`, all members move | One button for several shutters. | |
| `gallery-climate` | climate wide, target 22.0 with - / + | A thermostat. | done |
| `gallery-climate-states` | heating (flame), off, unavailable; target changing pending | Thermostat states. | |
| `gallery-sensor-sizes` | sensor 1x1 vs 2x1 with details + snippet | Sensor 1x1 and `size: 2x1`. | done |
| `gallery-sensor-options` | decimals 0/1/2, `unit:` override, `attribute:` | Format a sensor value. | |
| `gallery-sensor-states` | value, unavailable (n/a, dotted), loading (-) | A sensor when data is missing. | |
| `gallery-binary-sensor` | window open/closed, door, motion, battery (device_class wording) | Open or closed, on or off, in words. | |
| `gallery-weather` | weather 2x1 and 1x1, condition icons (sunny, cloudy, rain, snow, night) | Current weather. | |
| `gallery-weather-forecast` | weather with forecast (wide) | Weather with a forecast. | |
| `gallery-vacuum` | vacuum states: docked, cleaning, returning, error, unavailable | A robot vacuum. | |
| `gallery-graph-types` | `display: graph` with `type: line`, `bar`, `step` and snippet | Same data as line, bars or steps. | |
| `gallery-graph-options` | `hours`, `min/max`, `statistic`, `period` before/after | Graph options at a glance. | |
| `gallery-history-popup` | long press on a sensor opens the popup; snippet `history: 48` | Long press for history; `history: false` turns it off. | |
| `gallery-notifications` | notification count badge 0 / 3 + list popup | Home Assistant notifications. | |
| `gallery-calendar` | next event 2x1, empty day | The next calendar event. | |
| `gallery-media` | media player wide with art, playing / paused / off + popup | A media player. | |
| `gallery-media-art-off` | `art: false` vs default (side by side) | Turn the cover art off. | |
| `gallery-alarm` | alarm panel states (disarmed, armed home/away, triggered) + keypad popup | An alarm panel with a keypad. | |
| `gallery-lock-garage` | lock locked/unlocked/jammed, garage open/closed | Locks and garage doors. | |
| `gallery-energy` | energy 2x1 (kWh) | Energy today. | |
| `gallery-name-override` | same entity without and with `name:`; truncation with an ellipsis for a long name | Rename a widget. | |
| `gallery-icon-override` | `icon:` before/after (only if the option exists, see open question 2) | Pick another icon. | |
| `gallery-type-override` | `person.alice` with `type: sensor` | Show any entity as a sensor. | |
| `gallery-size-guide` | the 1x1, 2x1, wide, 2x2 and 3x2 blocks drawn as grey cells with which widget types allow which | Sizes each widget allows. | |

### G. Config (section "Configuration")

| File | Shows | Caption | Alt | Status |
|---|---|---|---|---|
| `config-to-screen` | Left: the `config.example.yaml` tab; right: the screen it draws; coloured-free arrows from each yaml item to its widget (numbers) | The yaml on the left makes the screen on the right. | Config snippet with arrows to the widgets it creates. | |
| `config-structure` | Nested boxes: file > tabs > widgets (entity, name, size) + groups + timezone | How config.yaml is built. | Nested boxes: tabs contain widgets. | |
| `config-indent` | Right vs wrong indentation (the usual typo) with the error row of Settings | One space can break it: the error names the line. | Correct and wrong yaml with the matching error message. | |
| `config-error-row` | The Settings Config row showing `tabs[0].widgets[2].entity, line 9` and the yaml line it points to | Read the config error. | Settings error row and the offending yaml line. | |
| `config-before-after-size` | Same tab with default sizes and with `size: 2x1` sensors | What `size` changes. | Two tabs, with and without size overrides. | |

### H. Troubleshooting (section "When something is wrong")

Each image: the real screen for that state 1:1 or at 60%, one badge, and a "Fix:" strip under it (1 to 3 steps).

| File | Shows | Caption | Status |
|---|---|---|---|
| `fix-offline` | stale clock icon + "Offline" label; fix: router range, Wi-Fi profile | Offline: the screen is the last known state. | |
| `fix-ha-unreachable` | Wi-Fi ok, HA error label; fix: URL / internal vs external, HA running | Wi-Fi works but Home Assistant does not answer. | |
| `fix-auth-error` | token rejected message; fix: create a new token, put into `secrets.yaml` | The token is wrong or revoked. | |
| `fix-entity-unavailable` | dotted frame widgets (sensor, light, cover) with the slash badge; fix: check the entity in HA | Dotted frame = Home Assistant says unavailable. | |
| `fix-low-battery` | battery icon at 10 %, charging; fix: USB-C data cable, mode Power off | Low battery. | |
| `fix-config-error` | see `config-error-row` (cross-linked, not a new image) | | |
| `fix-blank-screen` | decision tree: rocker, reset pinhole, charge | Screen stays blank. | |
| `fix-garbled` | garbled frame -> one full redraw -> power-cycle | Garbled after install. | |
| `fix-wont-wake` | wake methods per mode (Power off needs a 3 s hold) | It does not wake. | |
| `fix-sd-card` | FAT32 vs exFAT, files in the root not in a folder | Card not read. | |

## 3. Counts

Listed: A 5, B 8, C 11, D 7, E 8, F 29, G 5, H 9 (`fix-config-error` is a cross-link row, not an image) = **82 images**.
Batch 1 done: `device-buttons`, `screen-tab-anatomy`, `gallery-light-on-off`, `gallery-sensor-sizes`,
`gallery-climate` = 5. Beyond the task's list, the plan adds (marked above by being outside the task text):
`install-minimal-config`, `config-to-screen`, `config-before-after-size`, `sleep-before-after`,
`gallery-size-guide`, `screen-status-icons`, `screen-refresh-what-to-expect`, `device-yesno`,
`install-two-files`, `install-token`, `fix-sd-card`, `fix-wont-wake`.

## 4. Batches

1. Batch 1 (done): pipeline proof, 5 images.
2. Batch 2: C (screens), D (settings) and A rest: mostly mockup crops with badges; cheap.
3. Batch 3: F (gallery), G: assemble from accepted mockups (`widgets*.md`, `history-widget`, `sensor-details`).
4. Batch 4: B, E, H diagrams and fix images; replace guesses (`device-buttons`) after the user checks the device.

## 5. Notes on batch 1 geometry

- `screen-tab-anatomy`: screen at (40,30) in a 1040 x 900 canvas, copied from `tab-landscape.svg`; badge centres (screen
  coordinates): 1 (152,52), 2 (430,12), 3 (776,10), 4 (834,32), 5 (752,80), 6 (436,182), 7 (668,472); dashed boxes:
  slot (700,4,120,56) and free cells (392,420,552,104). Legend: 7 rows from y = 610, pitch 38.
- Fragments: widgets at (20,20) and (x+12 gap), labels at y = widget bottom + 26, panel below with 26 px line pitch.
  Widths: light 520, sensor 836, climate 592 (heights 358, 566, 300 or so).
- `device-buttons`: 1000 x 640; body 250 x 448 (3.8 px/mm); rocker (308,150,14,96): UP 150..182, PUSH 182..214,
  DOWN 214..246.

## Open questions

1. Rocker and port positions: layout.md 7.1 still says "assumption, to verify on hardware". Default: keep the drawing
   labelled "approximate" until the user sends one photo or confirms right edge, bottom USB-C, bottom SD, back reset.
2. Is there an `icon:` widget option? CONFIG.md lists none (task 111 mentions it). Default: skip `gallery-icon-override`.
3. Hold time for "Settings": layout.md says long push >= 600 ms; the drawing says "Hold". Default: keep "Hold" in images, the guide text says "about half a second".
4. Image size cap: 82 images x about 20 KB = about 1.7 MB for the site. Default: fine.
