# 004: Home Assistant client core

Status: todo
Depends on: 001
UI spec: none
Needs device: no

## Scope
Everything needed to read states and call services, testable without a network.

`lib/model`:
- `EntityState`: id, state string, available flag, `last_changed` (epoch s), and
  the domain-specific attributes below (typed, optional fields).
- `StateStore`: map id → `EntityState`, update from parsed results, report which
  ids changed in an update (the UI will redraw only those).

`lib/ha_client`:
- **Template request builder:** given a list of entity ids, build the JSON body for
  `POST /api/template` whose Jinja template renders compact JSON:
  `{"<id>": {"s": <state>, "lc": <last_changed epoch>, "a": {<attrs>}}, ...}`,
  using `tojson` for every value. Missing entities render as `{"s": null}`.
  Attributes per domain:
  - cover: `current_position`
  - light: `brightness`
  - climate: `current_temperature`, `temperature`, `current_humidity`, `hvac_mode`,
    `hvac_action`, `min_temp`, `max_temp`, `target_temp_step`
  - sensor: `unit_of_measurement`, `device_class`
  - weather: `temperature`, `humidity`, `temperature_unit`
  - vacuum: `battery_level`, `status`
  - all domains: `friendly_name`
- **Response parser:** template output → updates for `StateStore`. `unavailable`,
  `unknown` and missing entities set `available=false`. Bad JSON gives an error, no crash.
- **Service calls:** an `Action` enum per widget type → (`domain`, `service`, JSON body
  with `entity_id` as a list + extra data). Required actions:
  cover open/close/stop; light toggle; switch toggle; automation turn_on/turn_off;
  climate set_temperature (clamped to min/max, rounded to step); vacuum start/pause/return_to_base.
- **Transport interface:** `request(method, url, headers, body) -> {status, body, error}`.
- **Client:** uses the transport, sets `Authorization: Bearer` + JSON content type,
  **local-first fallback**: try `internal_url`; on connection error or timeout, try
  `external_url` if set; remember the working base URL and try it first next time
  (the "remembered" value is exposed so `app` can store it in RTC memory).
  HTTP 401 is reported as an auth error and does NOT trigger fallback.

`src/ha_transport_arduino.{h,cpp}`: transport using `HTTPClient` with
`WiFiClient` for http and `WiFiClientSecure` + the ESP-IDF CA bundle for https,
with connect/read timeouts. Build-only in this task.

Out of scope: Wi-Fi, history API, WebSocket, TLS session resumption (later).

## Files to touch
- lib/model/src/*
- lib/ha_client/src/*
- src/ha_transport_arduino.h, src/ha_transport_arduino.cpp
- test/test_ha_client/*, test/test_model/*

## Acceptance criteria
- [ ] Template builder output for a fixed id list matches a golden string (test), and the
      template is valid Jinja syntax by inspection (include it in the report).
- [ ] Parser handles a fixture covering every listed domain, unavailable and missing entities (tests).
- [ ] `StateStore` reports exactly the changed ids after an update (test).
- [ ] Every required action produces the right domain/service/body; a group action
      lists all member ids (tests).
- [ ] Climate set_temperature clamps and rounds to step (tests, incl. step 0.5).
- [ ] Fallback logic with a fake transport: internal OK → no external call; internal
      timeout → external used and remembered; 401 → no fallback, auth error (tests).
- [ ] `lib/` has no Arduino includes; `pio test -e native` and `pio run -e m5paper` pass.

## Notes
- Token permission for `/api/template` is verified later on a real HA instance.
