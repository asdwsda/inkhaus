# 038: Status bar link states and a nicer sleep indicator (spec, then firmware)

Status: done
Depends on: 024
UI spec: docs/ui/STYLE.md (status bar, sleep indicator) — revised by ui-designer
Needs device: yes (later)

## Scope
User feedback: the status bar is blank while connecting; no sign of fetching; values shouldn't be blanked
(use an "outdated" icon); the sleep indicators are ugly.
- Spec (ui-designer): status bar states and icons for: connecting (Wi-Fi), fetching, online ("updated
  HH:MM"), offline, auth error, outdated (small icon next to the time, shown when the last good update is
  older than 2× the refresh interval or the last fetch failed), demo. Each must fit the existing rects and
  update with a small partial refresh. A redesigned sleep indicator for both variants (button / touch) and
  both orientations: calmer, cleaner, e.g. a small "Zz"/moon glyph with a short text, no heavy black block.
- Firmware afterwards: implement the spec in src/render/screen.* and the status model.

## Acceptance criteria
- [ ] Spec + mockups (check_mockups 0 problems), the user approves the PNGs.
- [ ] Firmware implements it; on device: every state visible at the right time.

## Notes
- Device session 2026-10-30: user batches 4-5 OK (hotspot -> external and back to internal; demo icons; outdated chip; token rejected).
