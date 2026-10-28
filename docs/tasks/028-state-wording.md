# 028: State wording and icons in the firmware

Status: in-progress
Depends on: 026
UI spec: docs/ui/widgets.md section 11 (revision 6), STYLE.md section 8
Needs device: yes for the visual check (later)

## Scope
Implement widgets.md section 11 in lib/ui_state (wording tables, pure, tested) and the renderer:
binary_sensor by device_class (texts + icon pairs, widened state box), vacuum states + "!" at ≤ 20 %
when not docked, cover states with/without position, the generic fallback (`humanise`, n/a, "-").
New icons per the spec's list: add them to tools/gen_icons.py as tool-drawn designs (the mockups
don't contain them yet), regenerate src/render/icon_data.*, keep existing icons byte-identical.

## Files to touch
- lib/ui_state/src/widget_view.*, test/test_ui_state; src/render/widgets.cpp; tools/gen_icons.py,
  src/render/icon_data.*

## Acceptance criteria
- [ ] Every table row in widgets.md §11 has a native test (text + icon id).
- [ ] New icons generated; existing icon data unchanged (diff check).
- [ ] `pio test -e native` and the builds pass.
- [ ] On device (later): screenshots of a test config with one widget per class (m5paper-debug).

## Notes
- PC part done (2026-10-28): §11 wording tables in `toggleView`/`StateIcon`, humanise per 11.4,
  vacuum low battery ≤ 20 % when not docked, 22 tool-drawn icons (existing icons byte-identical).
  Icon designs are unreviewed: check them in the on-device screenshots.
