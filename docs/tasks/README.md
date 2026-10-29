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
| 008 | [HTTPS certificate bundle](008-https-ca-bundle.md) | in-progress | 004 |
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
| 020 | [Write config.yaml to the device over USB](020-provision-config-over-usb.md) | in-progress | 017 |
| 021 | [Wi-Fi connection module (fast reconnect)](021-wifi-net-module.md) | in-progress | 003 |
| 022 | [Draw the Settings screen](022-settings-screen-render.md) | in-progress | 007, 017 |
| 023 | [Wire Wi-Fi, Home Assistant and Settings into the app](023-wire-session.md) | in-progress | 017–022, 008 |
| 024 | [Sleep, wake and scheduled refresh](024-sleep-and-wake.md) | in-progress | 005, 016, 023 |
| 025 | [Config cache in internal flash](025-config-cache.md) | in-progress | 003, 020 |
| 026 | [Follow-ups from device session 1](026-device-session-1-followups.md) | in-progress | 023 |
| 027 | [TLS session resumption (researched, deferred)](027-tls-resumption.md) | todo | 008 |
| 028 | [State wording and icons](028-state-wording.md) | in-progress | 026 |
| 029 | [Wake log on the SD card](029-wake-log-sd.md) | in-progress | 024 |
| 030 | [Configurable sensor attributes (config + data)](030-sensor-attributes.md) | done | 003, 004, 017 |
| 032 | [Separate secrets.yaml from config.yaml](032-separate-secrets.md) | done | 003, 020, 025 |
| 033 | [Built-in demo showcase](033-demo-showcase.md) | done | 017, 028, 030 |
| 034 | ["Demo mode" setting](034-demo-mode-setting.md) | in-progress | 002, 007, 022, 033 |
| 035 | [Button mode: deep sleep with power hold](035-safe-button-sleep.md) | in-progress | 024 |
| 036 | [Fast taps: optimistic UI, keep-alive](036-fast-actions.md) | in-progress | 019, 023 |
| 037 | [Persist last known states](037-state-cache.md) | in-progress | 023, 025 |
| 038 | [Status bar states, sleep indicator redesign](038-status-bar-and-sleep-indicator.md) | in-progress | 024 |
| 039 | [Three wake modes, wake-mode fixes](039-wake-modes.md) | in-progress | 034, 035 |
| 040 | [`unknown` is not `unavailable`](040-unknown-state.md) | in-progress | 028, 036 |
| 041 | [Debug-log setting, Advanced page, splash](041-debug-log-and-splash.md) | done | 029, 034, 038 |
| 042 | [v1.0 release](042-release-1-0.md) | done | 041 |
| 043 | [Per-tab landscape columns](043-per-tab-columns.md) | done | 009, 011 |
| 044 | [Inkling branding (logo, splash, Tools)](044-inkling-branding.md) | done | 041 |
| 045 | [Brightness popup for dimmable lights](045-brightness-popup.md) | todo | 017, 036 |
| 046 | [History popup for sensors (research)](046-history-research.md) | todo | 030 |
| 047 | [Wi-Fi profiles, HA URL mode, Settings categories](047-wifi-profiles-and-url-mode.md) | todo | 021, 032, 026 |
