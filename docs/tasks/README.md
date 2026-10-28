# Tasks

| #   | Title | Status | Depends on |
|-----|-------|--------|------------|
| 001 | [Project skeleton](001-project-skeleton.md) | done | none |
| 002 | [Settings module](002-settings-module.md) | done | 001 |
| 003 | [config.yaml schema and parser](003-config-yaml.md) | done | 001 |
| 004 | [Home Assistant client core](004-ha-client-core.md) | done | 001 |
| 005 | [Power policy (logic only)](005-power-policy.md) | done | 002 |
| 006 | [UI layout engine](006-layout-engine.md) | done | 009, 010 |
| 007 | [Settings screen logic and layout](007-settings-screen.md) | in-progress | 010, 011 |
| 008 | [HTTPS certificate bundle](008-https-ca-bundle.md) | todo | 004 |
| 009 | [Config schema: tabs, widget size, group names](009-config-tabs.md) | done | 003 |
| 010 | [Orientation setting](010-orientation-setting.md) | done | 002 |
| 011 | [Layout engine update to the approved compact UI](011-compact-tabs-layout.md) | done | 006 |
| 012 | [binary_sensor device_class](012-binary-sensor-device-class.md) | done | 004 |
| 013 | [Hardware probe firmware](013-hardware-probe.md) | done | 001 |
| 014 | [Boot path on the device](014-boot-path.md) | done | 002, 013 |
| 015 | [E-ink persistence and refresh-mode experiment](015-epd-persistence-experiment.md) | done | 013, 014 |
| 016 | [Software restart hangs in panelSignature()](016-warm-boot-hang.md) | done | 014 |
| 017 | [Render the real UI on the device (demo data)](017-render-ui-demo.md) | in-progress | 011, 014, 016 |
| 018 | [Session logic: Wi-Fi, HA fetch, action queue](018-session-logic.md) | in-progress | 004, 005 |
| 019 | [Explicit on/off actions, actions before the first fetch](019-explicit-actions.md) | todo | 004, 018 |
