# 002: Settings module

Status: done
Depends on: 001
UI spec: none
Needs device: no

## Scope
User settings changed on the device (settings screen comes later), stored in NVS.
- `lib/settings`: pure logic.
  - `Settings` struct with:
    - `wake_mode`: enum `Button` | `Touch`. Default `Button`.
    - `refresh_interval_min`: one of 0 (off), 15, 30, 60, 120, 240. Default 60.
    - `idle_timeout_s`: one of 10, 20, 30, 60. Default 20.
  - For each setting, the list of allowed values in display order (the settings
    screen will cycle through them).
  - An abstract key-value store interface (get/set int by string key).
  - `load(store) -> Settings`: missing or invalid stored values fall back to the
    default for that field only.
  - `save(store, settings)`: writes only fields that changed.
  - A `schema_version` key; unknown future versions load as defaults.
- `src/settings_nvs.{h,cpp}`: NVS backend of the store interface using `Preferences`,
  namespace `settings`. Build-only in this task.

Out of scope: settings UI, applying settings (power policy is task 005).

## Files to touch
- lib/settings/src/*
- src/settings_nvs.h, src/settings_nvs.cpp
- test/test_settings/*

## Acceptance criteria
- [ ] Defaults are correct with an empty store (test).
- [ ] An invalid stored value (e.g. refresh 7) loads as the default for that field
      while other valid fields are kept (test).
- [ ] Save then load round-trips every allowed value of every field (test).
- [ ] Save doesn't write unchanged fields (test with a counting fake store).
- [ ] Unknown `schema_version` loads all defaults (test).
- [ ] `lib/settings` has no Arduino includes; `pio test -e native` passes; `pio run -e m5paper` passes.

## Notes
- Result: keys `schema_version`, `wake_mode`, `refresh_min`, `idle_s`; schema v1.
  A missing `schema_version` still loads the fields; save always stamps v1.
- Known low-severity gap: an NVS key holding the wrong type reads as 0, which is
  valid for `refresh_min` ("off") instead of the default. Only our code writes the
  namespace, so this is accepted for now.
- Verified in an isolated build because task 003 was mid-flight in the checkout.
