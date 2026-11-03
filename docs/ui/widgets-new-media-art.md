# Media player with album art (revision of widgets-new.md 4 / 4.1, proposal 2026-11-03)

Replaces sections 4 and 4.1 of `docs/ui/widgets-new.md` when accepted; everything not listed here is unchanged. Mockups (this folder, PNGs in `png/`):
`widgets-new-media-art-states.svg` (landscape 3x1: normal, no picture, loading, failed), `-tab-portrait.svg` (portrait 2x1: normal, truncated, all states incl. unavailable),
`-popup-landscape.svg`, `-popup-portrait.svg`. The art in the mockups is a 16-grey placeholder pattern.

**Art source.** `media_player.entity_picture` (JPEG/PNG via HA), decoded on the device to 16 greys with dithering. This is the only place the UI uses greys beyond STYLE 1's five:
the art rect is the exception, every other pixel (text, controls, edge, tiles) stays INK / PAPER / OFF. The art is always a fixed square; a non-square picture is centre-cropped.

## 1. Widget (wide only: 552 landscape, 508 portrait; height 104)

| Element | Box (widget-relative) | Font / grey |
|---|---|---|
| Art `w-N-art` | (12, 12, 80, 80) | 16 greys (0-15), GC16 |
| Art edge `w-N-art-edge` | (11, 11, 82, 82) outside the art | 1 px INK, no fill |
| Line 1 `w-N-l1` | (104, 21.6, W-344, 28.8), cy 36 | F24 INK, `...` |
| Line 2 `w-N-l2` | (104, 54.6, W-376, 28.8), cy 69 | F24 INK |
| Badge, buttons, sub-regions 1-3 | unchanged (W-264,57); (W-228 / W-152 / W-76, 20, 64, 64) | as 4 |
| Text block (sub-region 4) | (104, 12, W-344, 80) | opens the popup |
| Art (sub-region 5) | (12, 12, 80, 80) | also opens the popup |

Text room: 208 px (552) = about 14 characters of F24, 164 px (508) = about 11; longer text ends in `...` (mock row 1 of the portrait sheet). Rocker INNER stops: prev, play/pause (entry), next, text block (PUSH opens the popup); the art is touch-only (same action as the text block). Widgets with `art: false` in config keep the old layout (text at x 12).
A 4x1 landscape widget (740) uses the same rects, the extra width goes to the text.

**Art-area states** (the widget frame and buttons follow STYLE 9 / old section 4):
- *Art:* decoded picture + 1 px edge.
- *No picture* (no `entity_picture`, off, idle without cover): 2 px INK square tile (13, 13, 78, 78), PAPER fill, `volume` L (48) centred.
- *Loading* (fetch/decode running, shown at most until the answer): same tile with `hourglass` M (40) centred; the previous picture is not kept (wrong cover is worse than none).
- *Failed* (HTTP error, bad format, too large): tile, `volume` M (40) at (6, 8) of the art rect, `alert` S (24) at (48, 48); retried only when the picture URL changes or on the next wake.
- *Unavailable:* 2 px dotted OFF square (13, 13, 78, 78), PAPER, no icon (the widget warn badge already says it).

## 2. Popup (replaces 4.1 layout; same panel, frame, shadow, refresh as before)

**Landscape**, panel (40, 94, 880, 408), absolute rects:

| Element | Rect / position | Font |
|---|---|---|
| Art `w-90-art` | (56, 190, 200, 200); edge (55, 189, 202, 202) | 16 greys |
| Player name, close | x 56 cy 142; (840, 110, 64, 64) | as before |
| Title / artist / album | x 272, cy 206 / 254 / 288, max width 552 | F40 / F24 / F18 INK |
| Prev, play-pause, next | (272, 318, 96, 64), (380, 318, 96, 64), (488, 318, 96, 64) | glyph M |
| State word | x 604, cy 350 | F24 INK |
| Hint | right edge 904, cy 350 | F18 DIM, text shortened to `UP/DOWN volume` (PUSH = play/pause is on the button) |
| Volume row | unchanged: `-` (56, 406), track (132, 426, 472, 24), `+` (616, 406), value x 696, mute (840, 406) | |

Clearances: art to close 16, to the volume row 16.

**Portrait**, panel (16, 120, 508, 700) (was (16, 188, 508, 560); it grows 140 px), panel-relative:
name x 16 cy 48; close (428, 16, 64, 64); art (154, 88, 200, 200), edge 1 px outside; title x 16 cy 324 (F40, max width 476); artist cy 372; album cy 406;
transport (16, 432, 96, 64), (124, 432), (232, 432); state word x 344 cy 464; volume row y 520: `-` (16, 520), track (92, 540, 324, 24), `+` (428, 520);
row y 600: value x 16 cy 632, hint right edge 412 cy 632 (F18 DIM, `UP/DOWN volume`), mute (428, 600, 64, 64).
The popup art states are the same as the widget's, scaled: tile (x+1, y+1, 198, 198), `volume` L (48) centred, `hourglass` L centred, failed = `volume` L plus `alert` M at the tile's lower right.
Interactions, rocker and other states: as 4.1 (the art is not a touch target in the popup).

## 3. Refresh and redraw rule

- The art rect is redrawn **only when the picture changes**: the `entity_picture` URL (with its token) differs from the one shown, or the state moves to/from a no-picture state. Title, artist, play state and volume changes never touch it, and no other partial refresh may overlap it (the text/button rects above are disjoint from it).
- Art redraw = one **GC16 partial of the art rect only** (80x80 widget, 200x200 popup) after the 4-bit buffer is ready; the tile/placeholder is drawn first (`epd_fastest`) when loading starts. A GC16 art redraw counts as one quality refresh toward the ghost debt (STYLE 10).
- Tab change / full refresh: the art is part of the GC16 full refresh like everything else (decode before the flash, so there is no second pass). Popup open: the dirty rect `epd_quality` with the tile, then the art rect GC16 once decoded; if the same picture is already on screen in the widget, the 200 px version is decoded from the same download. Popup close: one `epd_quality` partial of the dirty rect as before (the widget art underneath is redrawn from its buffer in the same pass).
- Frozen screen: the art stays; no staleness marking (status bar rule unchanged). Memory: 80x80 and 200x200 at 4 bits = 3.2 KB + 20 KB.

## Open questions (recommended default)

1. Fetch once at 200x200 and downscale for the widget (recommended; one download per track). 2. Portrait popup grows to 700 px high (recommended; alternative: art 160 px, panel 620). 3. Art off per widget with `art: false` (recommended). 4. Text room is only 11-14 characters beside the art; accept `...` (recommended) or hide prev/next when art is on.

## Evidence

`tools/render_mockups.sh`, `tools/check_mockups.py docs/ui/widgets-new-media-art-*.svg`: 0 problems (4 files).
PNGs: `docs/ui/png/widgets-new-media-art-{states,tab-portrait,popup-landscape,popup-portrait}.png`.

User 2026-11-03: open questions 1-4 OK with the recommended defaults (one 200x200 fetch per track, portrait popup
700 px, per-widget `art: false`, accept `...`). Mockups accepted by the user 2026-11-03.
