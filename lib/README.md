# lib/ module convention

Pure logic lives here so it can be unit tested on the host (`pio test -e native`).

- One folder per module: `lib/<module>/src/...` (headers and sources together,
  public headers directly in `src/`). Example: `lib/settings/src/settings.h`.
- Pure C++17 only. **No Arduino headers** (`Arduino.h`, `M5Unified.h`, `Preferences.h`,
  `WiFi.h`, ...) and nothing from `src/` or `include/` that depends on them.
- Hardware access (NVS, SD, Wi-Fi, display) goes through small interfaces defined in
  the lib module and implemented in `src/`.
- Dependency direction: nothing in `lib/` depends on `src/`; modules may depend on
  other `lib/` modules as listed in docs/ARCHITECTURE.md.
- Tests for a module live in `test/test_<module>/`.
