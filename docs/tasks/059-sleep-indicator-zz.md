# 059: Sleep indicator as a bold "Zz" icon

Status: todo
Depends on: 038
UI spec: docs/ui/ (ui-designer updates the status bar / sleep indicator spec and mockups)
Needs device: yes (visual check)

## Scope
User (2026-10-30): the current sleep indicator (text, different per wake mode) is not liked. Replace it
with one simple sleep icon "Zz", made clearly visible: bold, e.g. black on white or a white "Zz" in a
black rounded badge; ui-designer proposes 2-3 variants, the user picks. No distinction between wake
modes (touch / button / both); it only says "sleeping, not usable right now". Also covers the low
battery sleep, unless the spec keeps a separate low-battery mark.
Keep the existing mechanics: drawn with a partial refresh of its rect right before sleep, cleared on wake.

## Files to touch
- docs/ui/ (spec + mockups, ui-designer), src/render (status bar / indicator), src/sleep.cpp,
  lib (sleep plan: drop the indicator variants if nothing else uses them), tests

## Acceptance criteria
- [ ] Spec with the chosen variant approved by the user (rendered mockups, both orientations).
- [ ] Native tests updated for the single indicator kind.
- [ ] Device: the Zz badge is visible at arm's length before sleep in all wake modes, cleared on wake
      without ghosts.

## Notes
- Coordinate with 058 (wake feedback): clearing the Zz badge quickly is a natural first wake feedback.
