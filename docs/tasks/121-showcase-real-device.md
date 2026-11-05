# 121: Showcase pictures with the real device's shape

Status: done
Depends on: 111 (guide visuals)
UI spec: docs/ui/showcase-device.md (accepted 2026-11-05)
Needs device: no

## Scope
User 2026-11-05: the showcase/landing drawings use a generic frame. Redraw the device frame with the real M5Paper geometry
(case size, screen offset, rocker and port positions; official M5Stack pictures and dimensions, see
scratchpad m5paper-physical.md notes), optionally a photo-realistic rendering. Applies to the landing page showcase,
the tour GIF and guide pictures that show the whole device. Screen content stays the real 960x540 UI.
Out of scope: photos taken of the user's device.

## Files to touch
- docs/ui/showcase-device-*.svg (new frame), docs/ui/showcase-*.svg, docs/guide/img/src, tools/render_* if needed, site/

## Acceptance criteria
- [ ] Frame dimensions match the official drawing (numbers listed in the spec with the source).
- [ ] Mockups rendered and checked (tools/render_mockups.sh, tools/check_mockups.py); user approves before replacing.
- [ ] `python3 site/build.py && python3 site/check.py` pass (weight limits; GIF shrunk if needed).

## Notes
- User 2026-11-05: clean frame for the guide and the tour GIF, photo frame for the landing page hero. Port the generator
  (scratchpad gen_device.py) into tools/showcase/ and wire it into the composites/tour generators.
- Done 2026-11-05: generator ported to tools/showcase/device_frame.py (frame() used by the composites and tour
  generators; body() also by docs/guide/img/gen_sources.py for device-buttons). Tour GIF: clean frame, 900 px wide
  (580 KB). Landing page: new static hero docs/ui/png/showcase-hero.png (photo frame, tools/showcase/build_showcase_hero.sh)
  above the GIF. Guide: only device-buttons shows the whole device (screen-orientation shows screens only).
