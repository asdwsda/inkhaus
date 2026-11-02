# Showcase tour GIF (task 075)

Build: `tools/showcase/build_showcase_tour_gif.sh` (runs `gen_showcase_tour.py`, renders frames, writes
`docs/ui/showcase-tour.gif`). Frames: `docs/ui/showcase-tour-NN.svg` (1600x900, w-1 frame, check_mockups clean).
Device frames, positions and margins are reused from `gen_showcase_composites.py` (portrait left,
landscape right, caption centred below). Screens are the existing mockups: tab-*, tab-*-asleep,
brightness-*, showcase-tab-* / showcase-popup-* (graph widgets and history popup, derived from
history-widget and history), settings-*. Tap feedback: filled-centre ring (white halo) at the press point;
long press adds a dashed outer ring. Progress dots under the caption. GIF: 1000 px wide, 24 colours, loops.

| Step | Caption | Frames (ms) |
|---|---|---|
| 1 | Asleep: the last state stays on screen | asleep (2400) |
| 2 | Wake: fresh states | asleep + tap ring (700), tab (2000) |
| 3 | Tap a light: on | tab + ring on Ceiling (2400) |
| 4 | Long press: brightness | tab + long-press ring (900), brightness popup (2200) |
| 5 | Shutters: open, stop or close | tab + ring on the shutter down button (2400) |
| 6 | Climate: set the temperature | tab + ring on the climate + button (2400) |
| 7 | Graph widgets on the tab | tab + ring on tab slot 2 (700), graph tab (2000) |
| 8 | Long press a sensor: 24 h history | graph tab + long-press ring (900), history popup (2400) |
| 9 | Settings on the device | settings overview + ring on Sleep & wake (2400) |
| 10 | Sleep & wake: choose how it sleeps | Sleep & wake page + ring on Sleep mode (2800) |
| 11 | Back to sleep | asleep (2400) |

Asleep frames (steps 1, 2, 11) show the STYLE 5.6 status pill (moon + OFF + refresh time), not the old Zz badge.
Settings frames use the current mockups; patched only: firmware version (from include/version.h), Keep awake shown as
On USB (overview summary gets ", USB awake"; landscape summary and page show Touch & rocker, 30 min as in its mockup).
Not in the tour: Firmware update row / Tools page, AWAKE indicator (GIF size; mentioned in the README text).

Known gaps: the mockups have no separate "after" state for steps 3, 5, 6 (the ring marks the target);
the landscape brightness mockup shows a different dashboard than the other landscape frames.
