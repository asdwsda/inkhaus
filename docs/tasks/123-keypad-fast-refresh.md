# 123: Alarm keypad: refresh only the code box per key press

Status: PC part done (device check open)
Depends on: 108
UI spec: docs/ui/widgets-new.md (alarm keypad section)
Needs device: yes (look + timing; PC part first)

## Scope
A key press redraws only the code box with `epd_fastest` (black/white dots), not the keypad through the normal
frame diff. Key press feedback stays as the spec says. Ghost debt rules (STYLE 10) still apply; the popup close
does its usual refresh.

## Files to touch
- src/render/secure_widget.*, src/present.* or the popup redraw path, lib/ui_state (dirty rect), test/

## Acceptance criteria
- [ ] Native tests: a digit press marks only the code box rect dirty.
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] Device check: log shows one fastest partial of the code box per key; keypad stays clean.
