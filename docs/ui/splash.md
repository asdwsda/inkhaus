# Splash screen (cold boot)

System screen, not config-driven, no touch targets, no rocker action. Values from
`docs/ui/STYLE.md` (centre-line rule: every text is placed by its centre line `cy`).
Mockups: `splash-portrait.svg` (540 x 960), `splash-landscape.svg` (960 x 540).
Rect = (x, y, w, h), screen coordinates.

## 1. Purpose and when it shows

Shown **once per cold boot** (reset reason POWERON/EXT and no valid screen record, or
the record's firmware differs; task 041), in the saved orientation (default
portrait), before the app draws its UI. Never on a wake from deep sleep (the last
frame is still on the panel). It tells the user the device is alive during the
Wi-Fi/config start-up (seconds) and which firmware runs. Data: app name and tagline
(constants), version = `FW_VERSION` (the same string as the Settings Firmware row).

## 2. Name proposal

Short name **Paper Remote**, tagline `for Home Assistant`, footer `M5Paper e-ink
remote`. (The long form "M5Paper HA Remote" stays the repo/product name; "Paper
Remote" fits on one line at F56 in both orientations, about 390 px wide.) The name
is a single constant, so it can be changed without a layout change as long as it
stays <= 440 px at F56 (about 12 characters); longer names step down to F40.

## 3. Refresh

Draw once, then **one GC16 full refresh** (flashes; 16 greys, so the 2 greys render
cleanly). Nothing is drawn after it by the splash itself. The app then draws its own
first screen with its normal first full refresh (the splash screen counts as
ghost-free). Hold the splash for at least 1.5 s so it is readable (the Wi-Fi/config
start-up normally takes that long anyway; do not add an artificial delay beyond it).
No partial updates, no progress bar, no animation.

## 4. Layout, portrait (540 x 960), white background

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Logo | (90, 170, 360, 400), see 6 | | black, #888 | house with a remote inside |
| Title | box (16, 650, 508, 80), centred x = 270 | 690 | F56 INK | `Paper Remote` |
| Tagline | box (16, 734, 508, 36), centred | 752 | F24 DIM (#444) | `for Home Assistant` |
| Divider | (235, 796, 70, 2) | | OFF (#888) | |
| Version | box (16, 822, 508, 36), centred | 840 | F24 INK | `v0.3.1` |
| Footer | box (16, 906, 508, 28), centred | 920 | F18 DIM | `M5Paper e-ink remote` |

The logo block (170..570) plus text (650..930) is centred about the screen middle;
margins >= 16.

## 5. Layout, landscape (960 x 540)

| Element | Rect | cy | Font / grey | Content |
|---------|------|----|-------------|---------|
| Logo | (70, 70, 360, 400) | | as portrait | same logo, same size |
| Title | x = 500, box (500, 165, 444, 80) | 205 | F56 INK, left | `Paper Remote` |
| Tagline | x = 500, box (500, 248, 444, 36) | 266 | F24 DIM, left | `for Home Assistant` |
| Divider | (500, 306, 70, 2) | | OFF | |
| Version | x = 500, box (500, 328, 444, 36) | 346 | F24 INK, left | `v0.3.1` |
| Footer | x = 500, box (500, 486, 444, 28) | 500 | F18 DIM, left | `M5Paper e-ink remote` |

## 6. Logo construction (360 x 400 local box, origin at the Logo rect; all shapes, no gradients)

A house with an e-paper remote in its window. Only #000, #888 (and white).
- Ground line: (10, 385)-(350, 385), 6 px, #888, round caps.
- Chimney: (254, 50, 32, 70) black.
- House body: rect (44, 176, 272, 194), 14 px black stroke, white fill.
- Roof: polyline (4,180) (180,24) (356,180), 18 px black stroke, round joins and caps.
- Remote (the M5Paper): rounded rect (115, 200, 130, 152), r 14, 8 px black stroke, white fill; screen rect (129, 214, 102, 110) 3 px #888 stroke.
- On the screen: toggle ON (pill (141, 226, 46, 24) black fill, white knob r 8 at (175, 238)) with a black stub (195, 236, 24, 5); toggle OFF (outline pill, 3 px black stroke, black knob r 7 at (154, 274)) with a #888 stub; a #888 line (141, 298, 78, 5). This echoes the app's rule: filled = on, outline = off.
- Home dot: black circle r 5 at (180, 338).
- Rocker: black rounded bar (245, 248, 14, 60) on the right edge, two white 2 px dividers at y = 268 and 288 (the 3-way button).

## 7. States

None: the splash is static. If the saved orientation cannot be read it is drawn in
portrait. Version shows `v?` if `FW_VERSION` is not defined (a dev build).
