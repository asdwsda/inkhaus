# 023: Wire Wi-Fi, Home Assistant and Settings into the app

Status: in-progress
Depends on: 017, 018, 019, 020, 021, 022 (phase 1), 008
UI spec: docs/ui/STYLE.md, layout.md, widgets.md, settings.md (revision 5)
Needs device: yes for the final check (with the user's config.yaml on SD)

## Scope
Turn the demo into the real app. `src/main.cpp` drives `lib/session::SessionMachine` and executes
its commands with the real modules:
- **Start:** wake reason from `lib/power` (for now only power-on and reset; sleep comes in a later
  task), battery, settings, wall clock (RTC; NTP via `configTime` once Wi-Fi is up, then set the
  RTC), UTC offset (constant for now, configurable later).
- **Commands:** `ConnectWifi` → `net::begin/poll` (task 021); `FetchStates` / `CallService` →
  `ha_client::HaClient` with `ArduinoHttpTransport` (task 004, HTTPS bundle from task 008), run on
  a separate FreeRTOS task (ARCHITECTURE.md: network I/O off the UI loop) with results posted
  back as events; `ShowStatus` → status bar (connection icon, "updated HH:MM", stale chip, auth
  error); `MarkWidgets` → pending/confirmed/failed visuals; `Render` → the existing present path;
  `Sleep` → for now: log the plan, `present::prepareSleep()`, `net::powerDown()`, and stay awake
  (real sleep is the next task).
- **Real states:** the StateStore comes from HA fetches; the demo states are only used when the
  config has no `connection:` section (keep "Demo" mode for that case).
- **Taps:** light/switch/automation via `uistate::toggleIntent` → explicit actions (task 019);
  cover/vacuum/climate via ha_client actions; climate set_temperature via the existing builder.
- **Settings (task 022 phase 2):** open on rocker long-push at TAB level and on a tap on the
  battery cluster; draw with `render::drawSettings`; route input to `settings_ui::SettingsScreen`;
  on leave save via `settings_nvs`, apply orientation (full redraw); "Refresh now" triggers a
  session fetch; idle 60 s in Settings. Add the `TEST_SETTINGS` build flag for screenshots.
- **TLS check hook (task 008):** the one-line hook for `-DTLS_CHECK`.
- **Logging:** `APP:` lines for every session command and result (no secrets: never the token,
  password, SSID beyond length + 2 chars, or response bodies; entity ids and states are fine).

## Files to touch
- src/main.cpp, new src/session_driver.{h,cpp} and src/net_task.{h,cpp} (or similar)
- src/render/screen.cpp (status bar states), lib/ui_state (Settings hook in AppState, marks), tests

## Acceptance criteria
- [ ] `pio test -e native` and the m5paper/probe/epdtest builds pass; new lib logic has tests.
- [ ] A host-side or native test drives the session driver with a fake HA (fake transport) through:
      connect → fetch → render → tap → call → confirm, and the offline/auth paths.
- [ ] Code review: network I/O never blocks the UI loop; no secret is logged (grep).
- [ ] On device with the user's config (later): states from HA shown; a lamp toggle works and is
      confirmed; offline over a dead network shows the offline status; Settings opens, saves and
      applies orientation; screenshots match docs/ui/png/settings-*.png.

## Notes
- The device is away until the user is back; build and test on the PC first.
- Follow-up from task 019 (do it in this task): `src/render/widgets.cpp:136` `drawToggle` uses
  `d.members[0]`; change it to `uistate::toggleSummary(d.members)` so a mixed group shows "On" if any
  member is on.
