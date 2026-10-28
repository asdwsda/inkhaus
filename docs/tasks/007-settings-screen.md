# 007: Settings screen logic and layout (PC-only part)

Status: done
Depends on: 010, 011
UI spec: docs/ui/settings.md (with docs/ui/STYLE.md, docs/ui/layout.md)
Needs device: no (drawing and the on-device check are a later task)

## Scope
Pure logic for the Settings screen in a new module `lib/settings_ui`, so the device task that
draws it only renders.
- **Layout:** rects for every element in portrait and landscape, exactly as settings.md
  sections 3–4 (rows, value text, dots, note slot, info rows, footer buttons, title, "unsaved"
  chip).
- **Model:** the edit buffer (behaviour rules in settings.md section 2): open copies the saved
  `settings::Settings`; each setting row cycles forward through its allowed values
  (`kOrientationValues` etc.); the "unsaved" state; the touch-wake warning in the note slot
  when wake mode is Touch.
- **Leave:** Back, or the idle timeout (settings.md): returns the settings to save (or
  "nothing changed", so no NVS write), and whether a full redraw / rotation change is needed.
- **Info rows:** formatting functions for firmware version, battery (% and V), HA connection
  (local / remote / offline + last success time), Wi-Fi SSID (+ "(not connected)"), config
  status (first error path + line, count). Inputs are plain structs; no hardware or HA access.
- **Input:** hit-testing for touch; rocker focus per settings.md section 5 (UP/DOWN move
  between rows and buttons, PUSH activates, long PUSH = Back).
- **Refresh hints:** which rect changed after each action (for the fastest/cleanup strategy).

## Files to touch
- lib/settings_ui/src/* (new)
- test/test_settings_ui/* (new)

## Acceptance criteria
- [ ] Every rect matches settings.md for both orientations, with tests written from the spec numbers.
- [ ] Cycling each row through all values and back to the start; "unsaved" appears after the first
      change and disappears when all values are back to saved (tests).
- [ ] Leaving with no change gives "no write"; with changes gives exactly the changed settings;
      an orientation change requests a full redraw (tests).
- [ ] Touch hits and rocker focus order per settings.md, including long PUSH = Back (tests).
- [ ] Info formatting covers offline, remote, a config with errors, and no config (tests).
- [ ] No Arduino includes in lib/settings_ui; `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Runs in a git worktree in parallel with task 017 (device). Don't touch src/ or other lib modules.
- Result: `lib/settings_ui` (layout for both orientations, the edit buffer, the unsaved chip, write-free
  leave, the orientation flag, refresh-now flow, info formatting, touch and rocker focus); 33 tests.
  Built in a worktree, merged as one commit.
- For the drawing task: hairlines use grey #888 (as the SVGs, not "OFF" as settings.md 3.3);
  borders are drawn inside the rects; the date format has no zero padding ("5.3. 09:05").
  Check that against the mockups when drawing.
