# 042: v1.0 release

Status: done
Depends on: 041 and the user's check of the current firmware
UI spec: none
Needs device: yes (final smoke test)

## Scope
- `FW_VERSION` 1.0.0; CHANGELOG.md (features in 1.0, known limitations: TLS ~4 s first request, touch controller
  awake during sleep, BLE ideas); README refresh (status → 1.0, screenshots from demo.yaml via m5paper-debug);
  all task files' statuses reconciled; local git tag `v1.0.0` (never pushed by agents).
- Final smoke test on the device with the user's config.
