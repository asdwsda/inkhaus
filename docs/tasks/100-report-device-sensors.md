# 100: Report the M5Paper's own sensors to HA

Status: in-progress
Depends on: 024, 079
UI spec: docs/ui/settings.md (new "Report to HA" row, added by ui-designer for task 100/101)
Needs device: yes (SHT30 read + HA states check; PC part first)

## Scope
Settings option "Report to HA" (Off | On, default On: user 2026-11-03; it only talks to the user's own HA). When On,
the device POSTs `/api/states/<entity>` (REST, existing token, existing HTTP client):
- `sensor.m5paper_battery` (%, attrs: `voltage_mv`, `charging`), `sensor.m5paper_temperature` (°C),
  `sensor.m5paper_humidity` (%), `sensor.m5paper_last_wake` (ISO 8601 timestamp, attr `reason`:
  scheduled|button|touch|boot). Proper `unit_of_measurement`, `device_class`, `state_class`, `friendly_name`.
- When: on scheduled wakes and on the first session after boot; post a sensor only if it changed
  (battery >= 1 %, temp >= 0.2 °C, hum >= 1 %) or its last post is > 6 h old; last_wake on every scheduled wake.
  Never on interactive wakes beyond that (no extra cost per tap). Requests go through the net task,
  after the state fetch, bounded by the existing session caps; a failure is logged, never retried in-session.
- New SHT30 driver (I2C 0x44, single-shot high repeatability, CRC check), read early in the wake before
  Wi-Fi/CPU heat up; result shared so task 101 (sleep screen) can show it. Device-side readings are
  local only; the sensors are a published-app-safe, user-opt-in feature.
- Out of scope: MQTT discovery / HA device registry (REST states vanish on HA restart until the next
  post; acceptable), history, a custom integration.

## Files to touch
- lib/ (pure: payload JSON builder, "should post" rule, SHT30 CRC/convert), test/ for them
- src/ (SHT30 I2C read, wiring into the session, settings key), lib/settings (new key + migration)
- Settings UI row per the spec (after ui-designer adds it)

## Acceptance criteria
- [ ] Native tests: payloads (units, classes, ISO time, attrs), change/age rule, SHT30 CRC + conversion
      against datasheet vectors.
- [ ] Default On (fresh install and migration). Setting Off: no POST, no SHT30 read cost beyond one read if task 101 needs it.
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] Log `HA: report sent=N skipped=M ms=..`; device-checks line (entities appear in HA, values sane).

## Notes
- Decision (architect, 2026-11-03): REST `/api/states` over MQTT. Reason: no new protocol/dependency,
  same token; the cost of states disappearing on HA restart is one scheduled wake at most.
