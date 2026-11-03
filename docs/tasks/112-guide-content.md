# 112: Guide content

Status: todo
Depends on: 110, 111; widgets 103-109 for their sections
UI spec: docs/ui/widgets.md, widgets-new.md, settings.md, sleep-screen.md
Needs device: no

## Scope
Write the guide pages in plain, friendly English for non-developers: short steps, one task per section, every option shown
with a picture and a copy-paste snippet, "what you'll see" and "if it doesn't work" for each step. Start with existing
features; add each new widget's section when its task (103-109) merges.
- Getting started: what you need, install (web flasher), SD card files (secrets.yaml + config.yaml, minimal working
  example), create the HA token (step list + link to HA docs), first boot, what the first screen means.
- Widgets: per type: what it's for, sizes, every option (table: key, values, default, picture), common recipes.
- Config reference generated or linted against the code (task 110 lint).
- Troubleshooting: symptom -> cause -> fix, with the screen picture.

## Files to touch
- docs/guide/, site/, tools/ (110, 111); docs/ui/ (111, ui-designer)

## Acceptance criteria
- [ ] Every config key and every Settings row is documented with default and effect.
- [ ] A new user can go from unboxing to a working dashboard using only the guide (dry-run review by tester as a newcomer).
- [ ] Site build + check pass.

## Notes
- User 2026-11-03: docs must be as user friendly as possible, part of the website, with widget screenshots and options.
- User 2026-11-03: HA token creation as text steps + link (no HA UI screenshots).
