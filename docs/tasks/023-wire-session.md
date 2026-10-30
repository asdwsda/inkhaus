# 023: Wire Wi-Fi, Home Assistant and Settings into the app

Status: done
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

### Progress (firmware-dev, PC side, 2026-10-28)
- Done: lib/app_logic (wall_time, mailbox, net_msgs, ha_worker + SyncNetPort, session_driver, tap_intent) with
  test/test_app_logic (15 tests, fake HA transport/Wi-Fi/clock); src/net_task, session_glue, settings_flow,
  main.cpp wiring, StatusInfo/status bar in src/render/screen.*, TEST_SETTINGS and TLS_CHECK hooks.
  Builds: m5paper (also with -DTEST_SETTINGS and -DTLS_CHECK), probe, epdtest; native 410 tests pass.
- Not done / needs other modules (lib/ui_state, lib/session): see report. Device check pending.
- Taps use the AppState optimistic pending result (fakeAction) as the request source; new pending entries
  are turned into session UserActions (lib/app_logic/tap_intent). No AppState edits were needed.
- PC part done (2026-10-28): lib/app_logic (wall time, mailbox, net messages, HA worker, session
  driver, tap intent; 15 tests), src/net_task (FreeRTOS, core 0), src/session_glue (Wi-Fi adapter, NTP →
  RTC), src/settings_flow, main.cpp wiring, status bar states. TLS_CHECK and TEST_SETTINGS hooks in.
- Round 2 (open): widget marks from the driver (`AppState::setMarks`) so pending lasts until confirmation
  and failed taps show the failed state; an action sink instead of pending-list diffing; the
  `toggleSummary` follow-up in widgets.cpp (still `members[0]`); the time zone per ARCHITECTURE.md
  (POSIX TZ, CET default); check that `present::begin()` frees the old canvases on re-begin.

### Round 2 progress (firmware-dev, 2026-10-28, PC only)
- Done: `AppState::setMarks/setMarksDriven/setActionSink` (lib/ui_state; `uistate::Mark`, `WidgetData::failed`),
  main.cpp uses the sink + `syncMarks()` (pending-list diff removed); widgets.cpp uses `toggleSummary` and
  shows the alert badge for failed; `timezone:` in lib/config (+tests, example yaml), TZ via setenv/tzset in
  main.cpp, offset from localtime (`UTC_OFFSET_S` removed); `present::begin()` deletes old sprites and logs
  `psram_free_before` / `psram_free`. native 417 pass; m5paper/probe/epdtest build. Device check pending.
- Note: the offset reaches `plan_sleep` via the Start event, so it is taken at session start.
- Follow-up after round 3: apply the NTP policy from ARCHITECTURE.md (2026-10-28): store the last sync
  epoch (NVS); sync only if the RTC is untrusted or the last sync is > 24 h old; never delay sleep.
- Device session 1 (2026-10-28, user's config on SD, phone hotspot):
  - Provisioning OK; Wi-Fi 828–948 ms scan, 438 ms cache hit; NTP synced, daily rule OK; HTTPS to the
    user's domain OK after the GlobalSign root fix (task 008); first HA fetch `200`, link=remote.
  - Bug (fix agent running at the end of the session, may have been cut off by unplugging): after the
    first fetch (changed=1) the widget showed "-" and no `cmd render` followed; after a rocker wake the
    state appeared. Likely the first render after a fetch is missing.
  - Fetch took 7.4 s: the local URL times out first over the hotspot. Follow-up: persist the remembered
    base URL across sleep (RTC memory) so the remote one is tried first.
  - Follow-up (user: "looks really ugly"): non-numeric sensor values are drawn in F56 in the wide layout.
    Draw text states in F24 and humanise raw HA states (`above_horizon` → "Above horizon").
  - A config with no entities makes the fetch fail with Parse; treat "nothing to fetch" as success.
  - TLS handshake ~6 s with debug logs; measure in the normal build; TLS session resumption later.
- "-" after the first fetch: not reproducible on the device (same firmware 98d89b9): render taken,
  member non-null, canvas and IT8951 memory both show the state. Open/watch: if it recurs, log right
  after FetchDone (store size, member pointer) and the panel record decision on that boot. Regression
  test `test_fetch_reaches_widget_data` added.
- Device session 2026-10-30 (tester): verified or superseded by later daily use; details in the session's device-checklist.md.
