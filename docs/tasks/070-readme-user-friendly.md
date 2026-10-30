# 070: User-friendly README

Status: todo
Depends on: 063, 065 (installer, website)
UI spec: none (ui-designer may review wording/images)
Needs device: no

## Scope
User (2026-10-30): make README.md as user friendly as possible for non-technical Home Assistant users.
- Top: one-paragraph pitch, hero images, "Get started in 4 steps" (buy hardware -> download the Inkhaus Installer ->
  install -> put config.yaml/secrets.yaml on the SD card or upload them), with links to the website and docs/FLASHER.md.
- A copy-paste minimal config.yaml + secrets.yaml example, how to create a long-lived token (with HA menu path).
- Plain-language troubleshooting/FAQ (no Wi-Fi, token rejected, offline, how to wake, battery).
- Move developer content (build, architecture, agent workflow, file tables) into docs/DEVELOPMENT.md; README links it.
- Fix outdated text (e.g. "a small status-bar icon shows how to wake it": now one Zz badge for all wake modes).
- No unmeasured battery-life claims.

## Files to touch
- README.md, docs/DEVELOPMENT.md (new), possibly docs/CONFIG.md if the config reference is long

## Acceptance criteria
- [ ] A non-developer can go from unboxing to a working dashboard with README alone (review by tester as a "new user" walkthrough).
- [ ] All links and image paths resolve (scripted check); no TODOs; matches the current firmware behaviour.
