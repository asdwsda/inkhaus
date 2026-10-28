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
