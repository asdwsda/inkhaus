# Showcase device frame with the real M5Paper shape (task 121, experiment)

Owner: ui-designer. Status: proposal, not accepted (nothing in `docs/ui/` or `docs/guide/` is replaced).
Mockups (1:1 px, DejaVu Sans, `check_mockups`: 0 problems; PNGs in `png/`):

| File | Variant | Canvas |
|---|---|---|
| `showcase-device-clean-landscape.svg` | clean vector, black 3 px outline, white fills | 1200x720 |
| `showcase-device-clean-portrait.svg` | same | 720x1200 |
| `showcase-device-photo-landscape.svg` | shaded: case gradient, thickness slab, metal rocker, shadow, glass sheen | 1200x720 |
| `showcase-device-photo-portrait.svg` | same | 720x1200 |

Screen content is `docs/ui/showcase-tab-landscape.svg` / `-portrait.svg`, copied in unchanged at 1:1
(the UI pixels are the real 960x540 / 540x960). Only the frame is new. Generator: not committed (see section 6).

## 1. Numbers and sources

Source A = official M5Stack dimension drawing (`m5img/size.jpg`, 3508x2480, "UNIT:mm"; labelled values).
Source B = same drawing, distances I measured on the full-size scan (scale 9.47 px/mm taken from the 67 mm
width), accuracy about +-0.3 mm. Source C = labelled product photo `m5img/p01.jpg` (which edge, order of parts).
Source D = docs.m5stack.com/en/core/m5paper (display area, case size in text).

| Value | mm | Source |
|---|---|---|
| Case (portrait) width x height x thickness | 67 x 118.6 x 10 | A, D |
| Case corner radius | 3 (side edge R0.5) | A |
| Display area (active) | 58.32 x 103.68 (4.7", 540x960) | D; B gives 58.4 x 103.3 |
| Bezel left = right | 4.34 (= (67 - 58.32) / 2); B: 4.3 | D, B |
| Bezel top / bottom | 4.3 / 10.62 (sum 14.92 = 118.6 - 103.68); B: 4.4 / 10.8 | D, B |
| Glass opening (drawn 0.9 mm larger than the display on each side) | 60.12 x 105.48 | B (inner line of the drawing) |
| Rocker ("dial", G37/G38/G39), right edge | centre 60 from the bottom, 12.5 long, protrudes about 1.0 | A (60, 12.5), B (bump height) |
| microSD slot, right edge | 24.7 to 35.8 from the bottom (11.1 long, centre 30.3), 1.3 deep | A (24.7, 11.1, 1.3) |
| USB-C, bottom edge | 6.2 to 15.3 from the left edge (9.1 wide) | B (not dimensioned in A) |
| PORT.A (HY2.0), bottom edge | 12 wide, right end 4.5 from the right edge (50.5 to 62.5) | A (12, 4.5); B 50.3 to 63.2 |
| PORT.B and PORT.C (HY2.0), left edge | 12 long each, 2 apart; lower one starts 11.4 from the bottom (11.4-23.4 and 25.4-37.4) | A (12, 2, 11.4) |
| Back: reset pinhole near the top, magnet, SHT30 vent | not on the front, not drawn | A, C |

Orientation (portrait, seen from the front, USB-C at the bottom): rocker and microSD on the right edge, PORT.B
and PORT.C on the left edge, USB-C and PORT.A on the bottom edge, top edge plain. This matches the labelled
photo C (PORT.B/C opposite the dial, PORT.A at the corner next to the dial edge, USB-C at the bottom).

Correction to the earlier architect note (`m5paper-physical.md`): the 12.5 mm slot centred at 60 mm is the
rocker (its bump is at the same height on the front view), not the microSD slot. The microSD slot is the
11.1 mm opening at 24.7 mm (a microSD card is 11 mm wide), lower on the same edge, as photo C shows.
Not verified: USB-C width (measured, B), the exact rocker protrusion (about 1 mm, B), the real case colour
(photo C: white), the 10 mm thickness in the shaded variant (only hinted by a slab offset of 0.9 mm).

## 2. Scale and canvas

- 9.259 px per mm (540 px / 58.32 mm), so the screen is exactly 540x960 (portrait) or 960x540 (landscape) and
  the case is 620.4 x 1098.2 px (portrait) or 1098.2 x 620.4 px (landscape).
- Canvas 1200x720 (landscape) / 720x1200 (portrait), case centred: landscape case origin (50.9, 49.8),
  portrait (49.8, 50.9). Margin about 50 px; the rocker protrudes 9 px into it.
- Landscape = the device turned 90 degrees counter-clockwise (the same convention as `device-orientation` in
  guide-visuals.md: rocker on top): top edge -> left, right edge -> top, bottom edge -> right, left edge -> bottom.

## 3. Layout (px, canvas coordinates)

Portrait, case origin (49.8, 50.9); `mm * 9.259`:

| Element | Rect (x, y, w, h) | Fill / stroke |
|---|---|---|
| Case, radius 28 | 49.8, 50.9, 620.4, 1098.2 | clean: grey 15 fill, black 3 px; shaded: gradient grey 15 to 12 |
| Glass opening, radius 9 | 81.7, 82.4, 556.6, 976.6 | clean: grey 15, `#888` 2 px; shaded: `#cfcfcf`, `#9a9a9a` |
| Screen (UI 1:1) | 90, 91, 540, 960 (rounded from 90.0, 90.7) | showcase-tab-portrait content, `#777` 1 px edge |
| Rocker, right edge, radius 8 | 655.4, 535.6, 24.1, 115.7 | clean: black, two white 2 px grooves at +-19 px; shaded: metal gradient, white highlight |
| microSD slot (right edge) | 660.9, 817.6, 9.3, 102.8 | grey `#555`, black 2 px outline |
| USB-C notch (bottom edge) | 107.2, 1138.0, 84.3, 11.1 | black |
| PORT.A notch (bottom edge) | 517.4, 1136.2, 111.1, 13 | black |
| PORT.B and PORT.C notches (left edge) | 49.8, 932.4, 13, 111.1 (B) and 49.8, 802.7, 13, 111.1 (C) | black |

Exact values are in the SVG (`dev-*` ids: `dev-case`, `dev-glass`, `dev-rocker`, `dev-screen`, `dev-screen-edge`,
`dev-sheen`); the geometry above is the mm table of section 1 times 9.259, so the SVG is the authority for
rounding. Landscape uses the same mm geometry through the transform of the rotated case (screen at 960x540,
origin (91, 90), rounded). Greys only: 0, 4 (`#444`), 5 (`#555`), 8 (`#888`),
and 12 to 15 for the shaded fills.

Shaded variant extras (only for pictures, never on the device): drop shadow (blur 14 px, offset 22/26 px,
opacity 55%), background `#f1f1f1` to `#c8c8c8` radial, case gradient `#fff / #ececec / #cdcdcd`, thickness slab
`#b4b4b4` offset 8 px, white inner highlight line, glass sheen 16% white fading out diagonally. It does not
change screen pixels by more than that sheen; for GIF palettes the sheen can be turned off (delete `dev-sheen`).

## 4. Interactions

None (picture only). The rocker and ports are labelled only in prose: the picture shows where they are.
For the guide drawing `device-buttons` the same mm table applies (that drawing already uses the M5Stack
positions; recheck it against the correction in section 1 about rocker versus microSD height).

## 5. States and use

- Screen content may be any 960x540 / 540x960 mockup (asleep, popup, settings); the frame does not depend on it.
- Landing page, GIF, guide pictures: swap the generic frame for this one in `gen_showcase_composites.py` and
  `gen_showcase_tour.py` only after the user accepts a variant. Cost to check then: the frame adds about 200 px
  of margin per device; the clean variant stays within the guide PNG limit (about 95 to 105 KB as RGB PNG
  before the 16-grey conversion, expected well under 60 KB after); the shaded one is the heavier (gradients,
  blur) and will dither in the 24-colour GIF, so the GIF should use the clean variant.
- Recommendation: clean for the guide and GIF, shaded for the landing page hero.

## 6. Notes

- Screen pixels are reused unchanged; do not scale the screen to fit a frame (the case is scaled to the screen).
- The generator is a throw-away script (`gen_device.py`: mm geometry, `matrix()` transform for the rotation,
  nested copy of the screen SVG). When accepted, port it into `tools/showcase/` next to the other generators.
