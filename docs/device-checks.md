# Device checks (pending verification)

Things built and unit-tested on the PC but not yet confirmed on the real M5Paper. Work through this list
with the device; tick items, and turn failures into tasks. Agents add every unverified assumption here,
tagged `UNVERIFIED` in code comments and task notes.

## Power and sleep
- [ ] Scheduled refresh (e.g. 30 min): the device wakes on the timer, fetches, redraws only changes,
      sleeps immediately (check with Debug log on: `/debug.log`, `/wakelog.csv`). Task 024/035.
- [ ] Wake modes: Button = rocker only, Touch = touch only, Touch & Button = both; indicator matches.
      Task 039.
- [ ] Standby current with the touch controller awake (USB-C meter / battery test). Task 035.
- [ ] Multi-day battery test with Debug log on. docs/battery-test-plan.md.
- [ ] Low-battery notice and sleep without alarm (temporarily raise the threshold). Task 024.

## Display
- [ ] Splash on a cold boot (after flashing; and on every reset once 1.1.0 lands). Task 041.
- [ ] Status bar states: connecting, fetching, updated, outdated (clock), offline, token rejected, demo.
      Task 038.
- [ ] "-" after the first fetch (once seen, never reproduced): watch `APP: fetch_applied`. Task 023.
- [ ] Settings value fit ("Touch & Button" in landscape). Fixed 85120f7.
- [ ] Landscape 4-column tabs match docs/ui/png/proposal-landscape-4col.png. Task 043.
- [ ] Landscape: 2 px rule between the tabs and the widgets (y=65..66), survives status/tab partial updates.
- [ ] New state wording icons (door, motion, …) look right. Task 028.

## Home Assistant
- [ ] Covers with `unknown` state: normal look, "-", Opening/Closing/Stopped on tap, shutter moves. Task 040.
- [ ] Timing of the first HTTPS request vs reused ones (`HTTP: ms=… reused=…`). Task 036/027.
- [ ] Rapid taps on two widgets: both reach HA in order (measure only; no change planned).
- [ ] Remote-URL-first after a hotspot wake (`link=remote`). Task 026.

## Settings and tools
- [ ] Tools page: Demo mode on/off reloads; Debug log on creates `/debug.log` + `/wakelog.csv`; "No SD card"
      note without a card. Task 041/034.
- [ ] README statements: rocker behaviour description; Settings shows the first config error.

## Added by later tasks
<!-- agents append here: "- [ ] <what> (task NNN, UNVERIFIED: <assumption>)" -->
- [ ] Splash on every reset-button/power-loss boot, then full redraw; none after deep-sleep wake (1.1.0, UNVERIFIED: ESP_RST_EXT/POWERON cover all cold boots)
- [ ] Real HA light returns `supported_color_modes` as a list in the template output and `dimmable()` is right for onoff-only vs dimmable lights; the 036 confirmation works for a `light.turn_on` with `brightness_pct` (task 045, UNVERIFIED: attribute arrives as a JSON array; the response carries the light with state "on"; `color_modes` is not in the config/state cache, so a cached boot falls back to the `brightness` attribute)
- [ ] `brightness_pct: 0` as `light.turn_off` is confirmed with expected state "off"; HA rounding of brightness_pct vs the shown value (task 045, UNVERIFIED)
- [ ] Wi-Fi profiles: Auto connects to the first listed network in range; forcing a profile connects without a scan; wrong forced profile fails cleanly (task 047, UNVERIFIED: async `WiFi.scanNetworks(true,false,false,300)` finds the APs within the 6 s budget)
- [ ] Per-profile fast reconnect: second connect to the same profile uses its cache (`NET: cache=hit`), switching profiles does not wear the other's cache (task 047, UNVERIFIED: skipping the scan when profile #1 connected last is fast enough)
- [ ] HA URL: home Wi-Fi -> internal, hotspot -> external (`APP: url_plan`, `HTTP: link=`), back home -> internal again (task 047, UNVERIFIED: per-SSID side in NVS "netbase" is read/written correctly from the net task on core 0)
- [ ] Settings wifi_profile (NVS string `wifiname`, by profile name) / ha_link (`halink`) persist; an old `wifiprof` index is migrated once to the name (a name no longer in secrets.yaml acts as Auto, no note); main.cpp calls `net::configure` once per session, so a change applies from the next session (task 047, UNVERIFIED)
- [ ] Auto with 2+ profiles: after a connect to the last profile (no scan) fails (AP off/out of range), `NET: direct_failed ... fallback=scan` follows and another profile connects (task 047 fix, UNVERIFIED: the failure is reported within the normal connect timeout)
- [ ] Scan hitting the 6 s timeout: `esp_wifi_scan_stop()` lets the following `WiFi.begin` connect (task 047 fix, UNVERIFIED)
- [ ] Brightness popup UI: long press (600 ms) on a dimmable light opens it, a short tap still toggles (now on release, ~tap length later); rocker long push on a focused dimmable light opens it; open/close are one quality refresh, value changes fast black/white without ghosts; steps, bar tap, presets and Off change the real light after ~400 ms; the hourglass clears when confirmed; idle 10 s closes (task 045 UI, UNVERIFIED: GT911 hold_begin/wasReleased events, the frame diff gives the spec's refresh regions, popup rect 45 % of the portrait screen stays under the 50 % big-change limit)
- [ ] Dimmable light tap toggles on release (~100–200 ms later than before): acceptable? (task 045)
- [ ] Brightness popup: popup is closed before sleep (frozen screen without it) and a modal tap outside neither toggles a widget nor switches tabs (task 045 UI, UNVERIFIED)
- [ ] Settings overview + 4 category pages (Display / Sleep & wake / Network / Tools), both orientations: rows, summaries, ring order, Back and LONG_PUSH go up one level, unsaved chip on every page, idle 60 s leaves entirely; page changes are GC16 fulls (task 047 UI, UNVERIFIED: the page draw reuses the old fonts/boxes; the Sleep & wake summary `Touch & Button, 240 min, 20 s` fits F18 or falls to `Both`)
- [ ] Changing Wi-Fi / HA connection in Settings applies on leaving: a fresh session starts after the tab screen is drawn (`NET:` / `APP: url_plan` lines), the Network page hints show the connected profile and link (task 047 UI, UNVERIFIED: restarting via startSession() while the old Wi-Fi is up reconnects cleanly; `net::activeProfileName()` is set when `wifiUp()`)
- [ ] Re-probe: a learned external is retried internal-first after 24 h (or the 20th session without a clock); on the hotspot that costs one failed internal try, then external again (task 047 fix, UNVERIFIED: NVS "netbase" key `l<hash>` blob + counter `sess` are written from the net task; the epoch passed to the plan is `time(nullptr)`)
- [ ] Network change in Settings while a tap action is pending: the action finishes first (up to 10 s), then the session restarts with the original wake reason; leaving Settings with the old Wi-Fi up disconnects cleanly and connects to the other profile (task 047 fix, UNVERIFIED: `WiFi.disconnect(false,false)` before `WiFi.begin` of another network is clean; a pending mark clears within the HTTP timeouts)
- [ ] `sess` counter in NVS "netbase" is not written while the clock is valid (only without a clock); an External learned without a clock is retried internal-first at the next session with a clock (task 047 fix, UNVERIFIED)
- [ ] History streaming GET (task 051, UNVERIFIED: `HTTPClient::writeToStream` into our Stream passes a chunked 100+ KB body in pieces, a short write aborts cleanly (cancel, 512 KB limit, 15 s deadline), and `HIST: fetch_ms bytes` is plausible over LAN and remote; the connection is dropped after each stream and the next normal call reconnects fine)
- [ ] History graph renderer matches the mockups (docs/ui/png/history-*.png) in both orientations: line+band, step lane with last-changes list, bars; unit shown as HA reports it (e.g. "°C"); only the greys GRID 0xD0D0D0 and MID 0x808080 appear (task 052, UNVERIFIED: 24-bit greys survive the canvas to GC16 refresh; `drawHistoryGraph` output positions match the SVGs, labels not clipped by the panel frame)
- [ ] History popup open/close (task 053, UNVERIFIED: GT911 LongTouch on a sensor/binary widget reaches `AppState::longPress` without a deferred tap; the popup opens, Loading appears after ~300 ms only when the fetch is slower, close button / tap outside / rocker push close it and abort the fetch; frame diff gives one quality refresh of the popup rect on open and close)
- [ ] History refresh modes (task 053, UNVERIFIED: `present::show` chooses per rect by pixel content, so the Loading frame over the tab's greys is a quality refresh, not the spec's `epd_fastest`; needs a per-show mode hint in src/present.* if that is too slow)
- [ ] `HIST: fetch_ms parse_ms bytes points err` line after each fetch, cached reopen shows at once (PSRAM), after a reboot from the SD file `/hist/<hash>.bin` (task 053, UNVERIFIED: SD mount/read/write between taps is fast enough and does not disturb the display bus; `/hist` is created on first store)
- [ ] Session stays awake while the popup is open and fetching; popup closes before sleep; a net-session restart during a fetch ends in the HA unreachable state (task 053, UNVERIFIED: 15 s fetch on the net task delays queued session fetches but does not break them)
- [x] Rocker push/long push on a sensor with history opens and closes the popup (task 053, user 2026-10-30)
