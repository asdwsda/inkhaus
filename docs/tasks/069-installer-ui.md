# 069: A more pleasant Inkhaus Installer UI

Status: in-progress
Depends on: 062, 064
UI spec: docs/ui/installer.md, variant C (minimal e-ink; user 2026-10-30)
Needs device: yes (user check of the real window)
Target: release 1.2.0

## Scope
User (2026-10-30): improve the installer's look for the next release. Keep the flow (detect device, pick version, Install,
optional YAML upload) and plain wording; make it friendlier: clear step layout, Inkhaus logo, device illustration/state,
large primary action, calm progress and success/error states, light and dark OS theme, Windows/macOS/Linux scaling.
Out of scope: new features.

## Files to touch
- docs/ui/installer.md + mockups (ui-designer), tools/flasher/src (app.rs, theme), embedded fonts/logo

## Acceptance criteria
- [x] Spec with mockups of every state (no device, ready, installing, done, error, YAML upload), variant approved by the user.
- [ ] Implementation matches the spec; cargo test/build pass; binary size growth noted.
- [ ] User check on the real window (install + YAML upload).
