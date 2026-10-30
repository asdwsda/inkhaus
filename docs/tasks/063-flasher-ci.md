# 063: Build the flasher for Windows, macOS and Linux in CI

Status: todo
Depends on: 061, 062
UI spec: none
Needs device: no

## Scope
Extend the release workflow: build tools/flasher for windows-x86_64 (.exe), macOS universal (arm64+x86_64, zipped .app or
binary) and linux-x86_64 (static/musl if egui allows, else AppImage), attach to the release. Unsigned; docs/FLASHER.md
explains the SmartScreen / Gatekeeper click-through and the CH9102 driver.

## Files to touch
- .github/workflows/release.yml, docs/FLASHER.md

## Acceptance criteria
- [ ] A pushed test tag produces all three binaries on the release; each starts on its OS (user check for Windows/macOS).
