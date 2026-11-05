# Tasks

| #   | Title | Status | Depends on |
|-----|-------|--------|------------|
| 001 | [Project skeleton](001-project-skeleton.md) | done | none |
| 002 | [Settings module](002-settings-module.md) | done | 001 |
| 003 | [config.yaml schema and parser](003-config-yaml.md) | done | 001 |
| 004 | [Home Assistant client core](004-ha-client-core.md) | done | 001 |
| 005 | [Power policy (logic only)](005-power-policy.md) | done | 002 |
| 006 | [UI layout engine](006-layout-engine.md) | done | 009, 010 |
| 007 | [Settings screen logic and layout](007-settings-screen.md) | done | 010, 011 |
| 008 | [HTTPS certificate bundle](008-https-ca-bundle.md) | done | 004 |
| 009 | [Config schema: tabs, widget size, group names](009-config-tabs.md) | done | 003 |
| 010 | [Orientation setting](010-orientation-setting.md) | done | 002 |
| 011 | [Layout engine update to the approved compact UI](011-compact-tabs-layout.md) | done | 006 |
| 012 | [binary_sensor device_class](012-binary-sensor-device-class.md) | done | 004 |
| 013 | [Hardware probe firmware](013-hardware-probe.md) | done | 001 |
| 014 | [Boot path on the device](014-boot-path.md) | done | 002, 013 |
| 015 | [E-ink persistence and refresh-mode experiment](015-epd-persistence-experiment.md) | done | 013, 014 |
| 016 | [Software restart hangs in panelSignature()](016-warm-boot-hang.md) | done | 014 |
| 017 | [Render the real UI on the device (demo data)](017-render-ui-demo.md) | done | 011, 014, 016 |
| 018 | [Session logic: Wi-Fi, HA fetch, action queue](018-session-logic.md) | done | 004, 005 |
| 019 | [Explicit on/off actions, actions before the first fetch](019-explicit-actions.md) | done | 004, 018 |
| 020 | [Write config.yaml to the device over USB](020-provision-config-over-usb.md) | done | 017 |
| 021 | [Wi-Fi connection module (fast reconnect)](021-wifi-net-module.md) | done | 003 |
| 022 | [Draw the Settings screen](022-settings-screen-render.md) | done | 007, 017 |
| 023 | [Wire Wi-Fi, Home Assistant and Settings into the app](023-wire-session.md) | done | 017–022, 008 |
| 024 | [Sleep, wake and scheduled refresh](024-sleep-and-wake.md) | in-progress | 005, 016, 023 |
| 025 | [Config cache in internal flash](025-config-cache.md) | done | 003, 020 |
| 026 | [Follow-ups from device session 1](026-device-session-1-followups.md) | done | 023 |
| 027 | [TLS session resumption (researched, deferred)](027-tls-resumption.md) | todo | 008 |
| 028 | [State wording and icons](028-state-wording.md) | done | 026 |
| 029 | [Wake log on the SD card](029-wake-log-sd.md) | done | 024 |
| 030 | [Configurable sensor attributes (config + data)](030-sensor-attributes.md) | done | 003, 004, 017 |
| 032 | [Separate secrets.yaml from config.yaml](032-separate-secrets.md) | done | 003, 020, 025 |
| 033 | [Built-in demo showcase](033-demo-showcase.md) | done | 017, 028, 030 |
| 034 | ["Demo mode" setting](034-demo-mode-setting.md) | done | 002, 007, 022, 033 |
| 035 | [Button mode: deep sleep with power hold](035-safe-button-sleep.md) | in-progress | 024 |
| 036 | [Fast taps: optimistic UI, keep-alive](036-fast-actions.md) | done | 019, 023 |
| 037 | [Persist last known states](037-state-cache.md) | done | 023, 025 |
| 038 | [Status bar states, sleep indicator redesign](038-status-bar-and-sleep-indicator.md) | done | 024 |
| 039 | [Three wake modes, wake-mode fixes](039-wake-modes.md) | done | 034, 035 |
| 040 | [`unknown` is not `unavailable`](040-unknown-state.md) | done | 028, 036 |
| 041 | [Debug-log setting, Advanced page, splash](041-debug-log-and-splash.md) | done | 029, 034, 038 |
| 042 | [v1.0 release](042-release-1-0.md) | done | 041 |
| 043 | [Per-tab landscape columns](043-per-tab-columns.md) | done | 009, 011 |
| 044 | [Inkling branding (logo, splash, Tools)](044-inkling-branding.md) | done | 041 |
| 045 | [Brightness popup for dimmable lights](045-brightness-popup.md) | done | 017, 036 |
| 046 | [History popup for sensors (research)](046-history-research.md) | done | 030 |
| 047 | [Wi-Fi profiles, HA URL mode, Settings categories](047-wifi-profiles-and-url-mode.md) | done | 021, 032, 026 |
| 048 | [History popup spec](048-history-popup-spec.md) | done | 046 |
| 049 | [History config keys](049-history-config.md) | done | 046 |
| 050 | [History data core (lib/history)](050-history-core.md) | done | 046 |
| 051 | [Streaming HTTP for history fetches](051-history-streaming-transport.md) | done | 046 |
| 052 | [History graph renderer](052-history-graph-renderer.md) | done | 048, 050 |
| 053 | [History popup (input, cache, refresh)](053-history-popup.md) | done | 049, 050, 051, 052 |
| 054 | [WebSocket statistics client and parser](054-history-ws-statistics.md) | done | 050, 051 |
| 055 | [Statistics source on the device](055-history-statistics-device.md) | done | 053, 054 |
| 056 | [Wi-Fi scan always fails](056-wifi-scan-fails.md) | done | 047 |
| 057 | [History finishing touches](057-history-finish.md) | done | 049, 052, 053, 055 |
| 058 | [Faster visual feedback on wake](058-faster-wake-feedback.md) | done | 024, 038 |
| 059 | [Sleep indicator as a bold "Zz" icon](059-sleep-indicator-zz.md) | done | 038 |
| 060 | [Split HTTPS timing (DNS / TCP / TLS / first byte)](060-net-timing-split.md) | done | 027 |
| 061 | [Release workflow: firmware assets on GitHub Releases](061-release-workflow.md) | done | – |
| 062 | [Flasher app (Rust, single binary)](062-flasher-app.md) | done | 061 |
| 063 | [Build the flasher for Windows, macOS and Linux in CI](063-flasher-ci.md) | done | 061, 062 |
| 064 | [Flasher: optional YAML upload to the SD card](064-flasher-yaml-upload.md) | done | 062, 020 |
| 065 | [Project website on GitHub Pages](065-project-website.md) | done | – |
| 066 | [Choose a new project name (Inkling may be taken)](066-project-name.md) | done | – |
| 067 | [Intermittent cold-boot hang after BOOT: controller_id=](067-cold-boot-hang.md) | done | – |
| 068 | [Demo mode shows history charts](068-demo-history.md) | done | 034 |
| 069 | [A more pleasant Inkhaus Installer UI](069-installer-ui.md) | done | 062, 064 |
| 070 | [User-friendly README](070-readme-user-friendly.md) | done | 063, 065 |
| 071 | [History graph widget](071-history-widget.md) | done | 049, 052, 053, 055 |
| 073 | [Showcase the history graph in the README and website](073-showcase-history.md) | done | 071, 070, 065 |
| 074 | [Graph widget redraw in one refresh](074-graph-single-refresh.md) | done | 071 |
| 075 | [Feature tour GIF as the only README image](075-feature-tour-gif.md) | done | 073 |
| 076 | [Smaller demo (4 tabs)](076-smaller-demo.md) | done | 033, 068, 071 |
| 077 | [Long press on another sensor breaks the graph widget](077-graph-popup-crosstalk.md) | done | 071, 074 |
| 078 | [Windows installer opens no console window](078-flasher-no-console.md) | in-progress | 069 |
| 079 | [Learn the battery limits from the wake stats](079-battery-limit-learning.md) | testing | 029, 035 |
| 080 | [Cap the Wi-Fi connect time per session](080-wifi-connect-cap.md) | testing | 047, 056 |
| 081 | [Stay awake on USB (setting) and manual sleep](081-usb-stay-awake-and-sleep-now.md) | done | 035 |
| 082 | [History graph cost in the wake log](082-history-cost-in-wakelog.md) | testing (night run after release) | 029, 071 |
| 083 | [Screen greys out while sleeping](083-panel-grey-drift.md) | done | 035, 082 |
| 084 | [Make full power-off sleep work on battery](084-power-off-sleep.md) | done | 035 |
| 085 | [Power settings: sleep mode, updates, keep awake, estimate, indicators](085-power-settings.md) | done | 084, 081, 079 |
| 086 | [Keep the current tab across sleep](086-keep-tab-across-sleep.md) | done | 071, 084 |
| 087 | [Firmware self-update](087-self-update.md) | done | 061, 084, 085 |
| 089 | [OTA install crashes in the TLS handshake](089-ota-tls-stack.md) | done | 087 |
| 090 | [Total time cap for an OTA job](090-ota-job-deadline.md) | PC done | 089 |
| 091 | [Scheduled wake switches back to the first tab](091-tab-lost-on-scheduled-wake.md) | in-progress | 086, 083 |
| 092 | [Grey drift: find the cause](092-grey-drift-root-cause.md) | in-progress | 083, 091 |
| 093 | [Task watchdog restart on a scheduled wake](093-wake-task-wdt.md) | PC done | 083, 091 |
| 094 | [Portrait 3-column grid](094-portrait-3-columns.md) | in-progress | 043 |
| 095 | [Clear grey drift during long awake sessions](095-awake-drift-cleanup.md) | PC done | 092 |
| 096 | [Keep supported_color_modes in the state cache](096-cache-color-modes.md) | done | 045 |
| 097 | [Set the clock from HA's HTTP Date header](097-clock-from-ha-date.md) | PC done | 008, 024 |
| 098 | [Native tests for UNVERIFIED device-check logic](098-native-tests-unverified.md) | done | 087, 090, 092 |
| 099 | [INIT clear on tab change (experiment)](099-init-clear-on-tab-change.md) | PC done | 092, 095 |
| 100 | [Report the M5Paper's own sensors to HA](100-report-device-sensors.md) | PC done | 024, 079 |
| 101 | [Sleep screen](101-sleep-screen.md) | PC done | 092, 100 |
| 102 | [Web flasher on the project site](102-web-flasher.md) | done | 061-065 |
| 103 | [Notifications widget](103-notifications-widget.md) | PC done | 101 |
| 104 | [Weather widget with forecast](104-weather-widget.md) | PC done | 045 |
| 105 | [Calendar widget](105-calendar-widget.md) | PC done | 045 |
| 106 | [Media player widget (without album art)](106-media-widget.md) | PC done | 045 |
| 107 | [Media album art](107-media-album-art.md) | PC part done | 106 |
| 108 | [Alarm panel, lock and garage widgets + confirm dialog](108-alarm-lock-widgets.md) | PC part done | 045 |
| 109 | [Energy widget](109-energy-widget.md) | PC done | 082 |
| 110 | [User guide on the website (structure and build)](110-user-guide-site.md) | done | 065, 102 |
| 111 | [Guide visuals: widget fragments, annotated screens, diagrams](111-guide-visuals.md) | todo | 110 (page layout), accepted mockups |
| 112 | [Guide content](112-guide-content.md) | todo | 110, 111; widgets 103-109 for their sections |
| 113 | [Build info in Settings](113-build-info-in-settings.md) | todo | 087 |
| 114 | [Clean the screen after a burst of taps (factory-app style)](114-tap-burst-cleanup.md) | done | 092, 095, 099 |
| 115 | [Slow taps after waking on battery](115-slow-taps-after-wake.md) | todo | 080, 093 |
| 116 | [User commands jump the net queue](116-user-commands-first.md) | PC part done | 115 |
| 117 | [Stress test before the 1.4.0 release](117-stress-test.md) | done | 116 |
| 118 | [Tap opens history on read-only widgets](118-tap-opens-history.md) | done | 046 |
| 119 | [Edit config from the web installer page](119-browser-config-editor.md) | todo | 102, 020 |
| 120 | [Show when the device is ready to send commands](120-ready-indicator.md) | todo | 116 |
| 121 | [Showcase pictures with the real device's shape](121-showcase-real-device.md) | todo | 111 |
| 122 | [Detect garage doors from device_class](122-garage-autodetect.md) | done | 108 |
| 123 | [Alarm keypad: refresh only the code box](123-keypad-fast-refresh.md) | todo | 108 |
| 124 | [Refactors after 1.4.0](124-refactor-survey.md) | todo | none |

## Open items (not yet tasks, 2026-10-30)
- 035: measure standby current before deciding on GT911 sleep (research in 035 Notes; no safe wake found).
- User-approved (2026-10-29): Settings overview summary wording (OPEN-S5) and long Wi-Fi names shrink then "..." (OPEN-S6).
- 056: a scan after a deep-sleep wake fails (aps=-2); rare now (scan only after a failed direct connect). Watch `event_ms=-1` in debug.log.

## Resume point (2026-11-03, usage limit)
- 104 merged (guide reference has `forecast`). Branches to merge next, both committed, PC part done:
  `worktree-agent-a7543ff6d34094f59` (103 notifications, regenerated icon_data: re-run tools/gen_icons.py on conflicts)
  and `worktree-agent-a10218e428c930f80` (106 media). Expect trivial conflicts in the widget enum, config_cache bound,
  ui_layout, widgets.cpp, app_state, main.cpp; add their new config keys to docs/guide/09-config-reference.md.
- Then: demo-data follow-ups for 103/104/106, tasks 105/107/108/109, guide visuals batch 2 (111), content (112).
