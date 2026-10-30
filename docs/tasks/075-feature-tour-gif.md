# 075: Feature tour GIF as the only README image

Status: in-progress
Depends on: 073
UI spec: docs/ui/showcase-tour.md (to write)
Needs device: no

## Scope
One animated GIF that shows the whole feature set as a UI flow, in the existing showcase style: device
frames, portrait and landscape side by side (user, 2026-10-30). It replaces all README images. The
README order is the intro, then the GIF, then "Get started", and it stays user friendly. The site gets
the same GIF as its hero and stays user friendly.

Flow (one caption per step, the same step on both devices):
1. Dashboard asleep, with the Zz badge.
2. Wake: fresh states.
3. Tap a light: on.
4. Long press: the brightness popup.
5. Shutters tab: open or close.
6. Climate: set temperature.
7. A graph widget on the tab.
8. Long press a sensor: the history popup.
9. Settings.
10. Back to sleep.
Out of scope: firmware changes and new features.

## Files to touch
- tools/showcase/ (generator, GIF build script), docs/ui/showcase-tour*.svg, docs/ui/png/,
  docs/ui/showcase-tour.gif, docs/ui/showcase-tour.md
- README.md, site/ (second step, after the GIF is approved)

## Acceptance criteria
- [ ] The GIF is built by a script in tools/showcase/, with no manual steps; each frame passes
  `tools/check_mockups.py`.
- [ ] Every screen matches the existing specs (tab, brightness, history, history-widget, settings).
- [ ] The GIF is 1000 px wide or less, under 1 MB, readable captions, about 2-3 s per step, loops.
- [ ] README: the GIF is the only image, right after the intro, and "Get started" follows. The site
  hero uses the GIF. `python3 tools/check_links.py`, `site/build.py` and `site/check.py` pass.
- [ ] The user approves the GIF and then the README and site.

## Notes
- Unused showcase files are deleted at the end (README and site must not reference them).
- Website hero uses the same GIF (user, 2026-10-30). Steps approved as listed.
