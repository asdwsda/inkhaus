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

## 2. Name

The app is named **Inkling** (user choice, ARCHITECTURE.md 2026-10-29), tagline `for Home
Assistant`, footer `M5Paper e-ink remote`. (The long form "M5Paper HA Remote" stays the
repo/product name.) "Inkling" at F56 is about 190 px wide, so it fits on one line in both
orientations. The name is a single constant; a name longer than 440 px at F56 (about 12
characters) steps down to F40.

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
| Logo | (90, 170, 360, 400), see 6 | | black only | variant D, toggle-i house (see 6, 8) |
| Title | box (16, 650, 508, 80), centred x = 270 | 690 | F56 INK | `Inkling` |
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
| Title | x = 500, box (500, 165, 444, 80) | 205 | F56 INK, left | `Inkling` |
| Tagline | x = 500, box (500, 248, 444, 36) | 266 | F24 DIM, left | `for Home Assistant` |
| Divider | (500, 306, 70, 2) | | OFF | |
| Version | x = 500, box (500, 328, 444, 36) | 346 | F24 INK, left | `v0.3.1` |
| Footer | x = 500, box (500, 486, 444, 28) | 500 | F18 DIM, left | `M5Paper e-ink remote` |

## 6. Logo construction (variant D, "toggle-i house"; source `docs/ui/logo-inkling.svg`, 100 x 100 viewBox)

Black on white, no greys, no gradients. On the splash the symbol is drawn at 360 x 360
(scale 3.6) centred in the 360 x 400 Logo rect (offset (0, 20)); the same symbol is used
for the 48 px icon. Coordinates below are in the 100 x 100 symbol; every stroke is 9.
- House: open polyline, 9 px black stroke, round joins and caps: (74,90) (88,90) (88,46) (50,12) (12,46) (12,90) (26,90). The gap (26..74) in the floor is the doorway.
- Dot of the i (the ink drop): black, round part r 9 at (50,41), tip at (50,28.3); path `M50 28.27 L56.36 34.64 A9 9 0 1 1 43.64 34.64 Z`.
- Stem of the i (toggle, ON): black pill (38, 55, 24, 35), r 12, standing on the floor line (bottom y 90) in the doorway; white knob r 7 at (50,67). Knob ring = 5, stem width 24 = about 2.7 x the house stroke.
- Clearances: drop tip to roof underside about 6; drop to stem 5; stem to floor ends about 8.
- Filled = on, as in the app: the toggle shows the ON state.

## 7. States

None: the splash is static. If the saved orientation cannot be read it is drawn in
portrait. Version shows `v?` if `FW_VERSION` is not defined (a dev build).

## 8. Logo status (task 044)

**Chosen: D** (variant C one-stroke house with variant B toggle-i in the doorway),
sheet `docs/ui/logo-variants.svg`, standalone mark `docs/ui/logo-inkling.svg` (100 x 100,
for the firmware icon generator; render at 360 px for the splash and at 48 px for icons).
It reads at 48 px (dot, stem and knob stay distinct). Everything else on the splash is unchanged.
