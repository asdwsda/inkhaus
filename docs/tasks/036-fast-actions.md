# 036: Fast taps: optimistic UI, keep-alive HTTPS, confirm from the call response

Status: in-progress
Depends on: 019, 023
UI spec: docs/ui/widgets.md (pending/failed states; pending visual replaced by optimistic)
Needs device: yes (timing check)

## Scope (ARCHITECTURE.md 2026-10-29)
- **Optimistic UI** (lib/ui_state, lib/app_logic): a tap immediately shows the requested state (toggle on/off,
  cover moving direction, climate target) and is refreshed with the fastest mode; no hourglass. On a
  `Failed` mark: revert to the last real state + failed badge. Demo mode behaves the same.
- **Confirm from the service call response**: HA's `POST /api/services/<d>/<s>` returns the list of
  changed states; parse it (lib/ha_client, same attribute rules as the template parser) and feed it to
  the store as the confirmation. Only if the entity isn't in the response: one confirmation fetch
  (shorter delay, e.g. 300 ms) as today.
- **Keep-alive** (src/ha_transport_arduino, lib/ha_client): one `WiFiClientSecure`/HTTP connection reused
  for all requests of a wake (`setReuse(true)`, `Connection: keep-alive`), reconnect transparently on
  failure; closed before sleep. Log per request: `HTTP: ms=… reused=0|1 link=…`.
- **Order on a button wake with a queued tap**: Wi-Fi → call (actions first, as today) → then the
  initial fetch. No full fetch before the call.
- Timing logs to measure: tap → call sent, call → response, response → screen.

## Files to touch
- lib/ha_client, src/ha_transport_arduino.*, lib/session, lib/app_logic (session_driver, ha_worker),
  lib/ui_state (optimistic view), src/net_task.*, tests. NOT src/main.cpp beyond a few lines
  (task 037 hooks there too).

## Acceptance criteria
- [ ] Native tests: optimistic view and revert; service-response parsing and confirmation; no fetch when
      the response contains the entity; keep-alive reuse logic with a fake transport (one connect for
      several requests).
- [ ] `pio test -e native` and the builds pass.
- [ ] On device: tap → widget flips immediately; HA state switches clearly faster than before (log timings).

## Notes
